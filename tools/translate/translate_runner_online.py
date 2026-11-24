#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
# Modified by Andrea Costantino on 2026-11-24: use the official Libre API
# and explain non-JSON instance responses without exposing their contents.
"""Online translation helper. stdin: text/HTML; stdout: a JSON result or error."""
import argparse
import logging
import os
import sys
import time

from helper_common import (
    TranslationError, configure_logging, detect_language, translate_content,
    translate_html_carefully as translate_html_carefully, write_result,
)


def normalize_language(code, supported):
    values = list(supported.values())
    exact = next((value for value in values if value.lower() == code.lower()), None)
    if exact:
        return exact
    match = next((value for value in values if value.split("-")[0].lower() == code.lower()), None)
    if match:
        return match
    raise TranslationError(f"This provider does not support language {code}.")


class LibreTranslator:
    """The existing Libre endpoint also supports instances without an API key."""
    def __init__(self, source, target, api_key=None):
        self.source, self.target, self.api_key = source, target, api_key
        self.url = os.environ.get("LIBRE_TRANSLATE_URL", "https://libretranslate.com").rstrip("/") + "/translate"

    def translate(self, text):
        import requests
        payload = {"q": text, "source": self.source, "target": self.target, "format": "text"}
        if self.api_key:
            payload["api_key"] = self.api_key
        with requests.post(self.url, json=payload, timeout=20) as response:
            if response.status_code == 429:
                raise TranslationError("LibreTranslate is rate-limiting requests. Try again later or use Argos offline.")
            if not response.ok:
                raise TranslationError(f"LibreTranslate returned HTTP {response.status_code}. Check the configured instance and API key.")
            try:
                result = response.json()
            except ValueError as error:
                raise TranslationError(
                    "LibreTranslate did not return JSON. Check the configured instance URL "
                    "and API key, or use Argos offline."
                ) from error
        translated = result.get("translatedText") if isinstance(result, dict) else None
        if not isinstance(translated, str):
            raise TranslationError("LibreTranslate returned an invalid response.")
        return translated


def translate_online(text, target_lang, provider, is_html=False, api_key=None,
                     source_lang=None, preserve_format=True):
    try:
        from deep_translator import GoogleTranslator, MyMemoryTranslator
        from deep_translator.constants import MY_MEMORY_LANGUAGES_TO_CODES
        from deep_translator.exceptions import RequestError, TooManyRequests
        import requests

        if not text or not text.strip():
            raise TranslationError("No message text was provided.")
        if provider == "google":
            # Let Google detect each text node so mixed-language emails still work.
            translator = GoogleTranslator(source=source_lang or "auto", target=target_lang)
            limit = 4999
        elif provider == "mymemory":
            source = source_lang or detect_language(text, is_html)
            translator = MyMemoryTranslator(
                source=normalize_language(source, MY_MEMORY_LANGUAGES_TO_CODES),
                target=normalize_language(target_lang, MY_MEMORY_LANGUAGES_TO_CODES))
            limit = 499
        elif provider == "libre":
            translator = LibreTranslator(source_lang or "auto", target_lang,
                                         api_key or os.environ.get("LIBRE_API_KEY"))
            limit = 4999
        else:
            raise TranslationError(f"Unsupported translation provider: {provider}.")

        for attempt in range(3):
            try:
                translated = translate_content(translator, text, is_html, preserve_format, limit)
                return {"translated": translated}
            except TooManyRequests as error:
                # Retrying an explicit quota rejection only adds load.
                raise TranslationError(
                    f"{provider.title()} is rate-limiting translation requests (HTTP 429). "
                    "Wait before trying again, or select Argos Translate for offline translation."
                ) from error
            except (requests.Timeout, requests.ConnectionError, RequestError) as error:
                if attempt == 2:
                    raise TranslationError(
                        f"Could not reach {provider.title()}. Check your connection or use Argos Translate offline."
                    ) from error
                time.sleep(2 ** attempt)
    except ImportError as error:
        return {"error": "Online translation dependencies are missing. Run evolution-translate-setup --online-only.",
                "error_code": "dependencies", "detail_type": type(error).__name__}
    except TranslationError as error:
        return {"error": str(error)}
    except Exception as error:
        # Provider exceptions may echo the input or credentials: expose only the type.
        logging.debug("Provider failure type: %s", type(error).__name__)
        return {"error": f"{provider.title()} translation failed ({type(error).__name__}). Check Translate Settings or use Argos offline."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", default="en")
    parser.add_argument("--source")
    parser.add_argument("--provider", default="google", choices=["google", "mymemory", "libre"])
    parser.add_argument("--html", dest="is_html", action="store_true")
    parser.add_argument("--text", dest="is_html", action="store_false")
    parser.add_argument("--no-preserve-format", dest="preserve_format", action="store_false")
    parser.add_argument("--debug", action="store_true")
    parser.add_argument("--api-key")
    args = parser.parse_args()
    configure_logging(args.debug)
    result = translate_online(sys.stdin.read(), args.target, args.provider, args.is_html,
                              args.api_key, args.source, args.preserve_format)
    return write_result(result)


if __name__ == "__main__":
    sys.exit(main())
