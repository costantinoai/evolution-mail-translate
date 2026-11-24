#!/usr/bin/env python3
"""Argos translation helper. Message text is processed locally."""
import argparse
from contextlib import redirect_stdout
import logging
import os
import sys

from gpu_utils import setup_gpu_acceleration
from helper_common import (
    TranslationError, configure_logging, detect_language, translate_content,
    translate_html_carefully as translate_html_carefully, write_result,
)


def auto_download_model(from_code, to_code, debug_func=None):
    import argostranslate.package as packages
    if debug_func:
        debug_func(f"Downloading translation model {from_code} → {to_code}")
    packages.update_package_index()
    package = next((item for item in packages.get_available_packages()
                    if item.type == "translate" and item.from_code == from_code and item.to_code == to_code), None)
    if package is None:
        raise TranslationError(f"No Argos model is available for {from_code} → {to_code}.")
    packages.install_from_path(package.download())
    return True


def translate_offline(text, target, is_html, install_on_demand=True,
                      source_lang=None, preserve_format=True):
    if not text or not text.strip():
        raise TranslationError("No message text was provided.")
    if os.environ.get("TRANSLATE_FAKE_UPPERCASE") == "1":
        class FakeTranslator:
            def translate(self, value):
                return value.upper()
        return translate_content(FakeTranslator(), text, is_html, preserve_format)
    setup_gpu_acceleration()
    import argostranslate.translate as argos
    # Argos logs entire paragraphs at INFO, even when the helper is not debugging.
    logging.getLogger("argostranslate.utils").disabled = True
    source = source_lang or detect_language(text, is_html)
    if source == target:
        return text

    def find_translation():
        languages = argos.get_installed_languages()
        origin = next((language for language in languages if language.code == source), None)
        destination = next((language for language in languages if language.code == target), None)
        if origin is None or destination is None:
            return None
        try:
            return origin.get_translation(destination)
        except (RuntimeError, ValueError, AttributeError):
            return None

    translator = find_translation()
    if translator is None and install_on_demand:
        auto_download_model(source, target)
        # Argos caches its installed language list; clear it if this release exposes a cache.
        if hasattr(argos.get_installed_languages, "cache_clear"):
            argos.get_installed_languages.cache_clear()
        translator = find_translation()
    if translator is None:
        raise TranslationError(
            f"Argos has no installed translation model for {source} → {target}. "
            "Enable Install models on demand in Translate Settings, or install this language pair first."
        )
    return translate_content(translator, text, is_html, preserve_format)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", default="en")
    parser.add_argument("--source")
    parser.add_argument("--html", dest="is_html", action="store_true")
    parser.add_argument("--text", dest="is_html", action="store_false")
    parser.add_argument("--no-preserve-format", dest="preserve_format", action="store_false")
    parser.add_argument("--install-on-demand", dest="install_on_demand", action="store_true", default=True)
    parser.add_argument("--no-install-on-demand", dest="install_on_demand", action="store_false")
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()
    configure_logging(args.debug)
    try:
        with redirect_stdout(sys.stderr):
            translated = translate_offline(sys.stdin.read(), args.target, args.is_html,
                                           args.install_on_demand, args.source, args.preserve_format)
        result = {"translated": translated}
    except ImportError:
        result = {"error": "Argos translation dependencies are missing. Run evolution-translate-setup without --online-only."}
    except TranslationError as error:
        result = {"error": str(error)}
    except Exception as error:
        result = {"error": f"Offline translation failed ({type(error).__name__}). Check the installed Argos model."}
    return write_result(result)


if __name__ == "__main__":
    sys.exit(main())
