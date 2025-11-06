#!/usr/bin/env python3
"""
translate_runner.py
Argos Translate runner: reads plain text from stdin and writes translated
plain text to stdout. Options:
  --target <lang>  target ISO 639-1 (default: en)
  --html | --text  hint whether input is HTML (best-effort)
  --install-on-demand | --no-install-on-demand  enable/disable auto-download of models
  --debug          enable debug logging to /tmp/translate_debug.log

If argostranslate/translate_html/langdetect are unavailable or models are
missing, falls back to a no-op (echo) translation, so the pipeline works.
"""

import argparse
import os
import sys
import json

# Debug logging support
DEBUG_MODE = False
DEBUG_LOG_FILE = "/tmp/translate_debug.log"

def debug_log(msg):
    """Log debug message if debug mode is enabled"""
    if DEBUG_MODE:
        try:
            with open(DEBUG_LOG_FILE, "a") as f:
                f.write(msg + "\n")
        except Exception:
            pass  # Silently ignore debug logging errors

# Import shared GPU utilities
from gpu_utils import setup_gpu_acceleration

# Set up GPU acceleration before importing argostranslate
setup_gpu_acceleration(debug_log_func=debug_log)

def auto_download_model(from_code: str, to_code: str, debug_func=None) -> bool:
    """
    Auto-download a translation model if it's not installed.

    Args:
        from_code: Source language code (ISO 639-1)
        to_code: Target language code (ISO 639-1)
        debug_func: Optional debug logging function

    Returns:
        True if model was successfully downloaded and installed, False otherwise.
    """
    log = debug_func if debug_func else lambda msg: None

    try:
        import argostranslate.package as argospkg

        log(f"Auto-download requested for {from_code} → {to_code}")

        # Update package index to get latest available models
        print(f"[translate] Updating package index...", file=sys.stderr)
        log("Updating argostranslate package index...")
        argospkg.update_package_index()
        log("Package index updated successfully")

        # Find the specific translation package
        available_packages = argospkg.get_available_packages()
        log(f"Found {len(available_packages)} available packages")

        target_package = None
        for pkg in available_packages:
            if pkg.type == "translate" and pkg.from_code == from_code and pkg.to_code == to_code:
                target_package = pkg
                break

        if not target_package:
            print(f"[translate] ERROR: No translation model found for {from_code} → {to_code}", file=sys.stderr)
            print(f"[translate] Available language pairs can be found at: https://www.argosopentech.com/argospm/index/", file=sys.stderr)
            log(f"No package found for {from_code} → {to_code}")
            return False

        # Download and install the package
        print(f"[translate] Downloading {from_code} → {to_code} translation model...", file=sys.stderr)
        log(f"Downloading package: {target_package.package_name}")

        download_path = target_package.download()
        log(f"Downloaded to: {download_path}")

        print(f"[translate] Installing {from_code} → {to_code} model...", file=sys.stderr)
        argospkg.install_from_path(download_path)

        print(f"[translate] ✓ Successfully installed {from_code} → {to_code} translation model", file=sys.stderr)
        log(f"Successfully installed {from_code} → {to_code}")
        return True

    except ImportError as e:
        print(f"[translate] ERROR: Failed to import argostranslate: {e}", file=sys.stderr)
        log(f"ImportError in auto_download_model: {e}")
        return False
    except OSError as e:
        print(f"[translate] ERROR: Network or file system error: {e}", file=sys.stderr)
        log(f"OSError in auto_download_model: {e}")
        return False
    except Exception as e:
        print(f"[translate] ERROR: Failed to auto-download model: {e}", file=sys.stderr)
        log(f"Exception in auto_download_model: {e}")
        return False


def translate_html_carefully(translator, html_content: str) -> str:
    """
    Translate HTML content while preserving all HTML structure, tags, attributes, and comments.
    Only translates text nodes, leaving everything else untouched.
    """
    try:
        from bs4 import BeautifulSoup, NavigableString, Comment, Doctype
        import re

        # Parse HTML with html.parser (better for email HTML with malformed content)
        soup = BeautifulSoup(html_content, 'html.parser')

        def should_translate_text(text: str) -> bool:
            """Check if text is worth translating (not just whitespace or very short)"""
            cleaned = text.strip()
            # Don't translate if it's just whitespace, very short, or looks like code
            if not cleaned or len(cleaned) < 2:
                return False
            # Don't translate if it's all numbers/symbols
            if re.match(r'^[\d\s\W]+$', cleaned):
                return False
            return True

        def translate_element(element):
            """Recursively translate text nodes in an element"""
            if isinstance(element, NavigableString):
                # Skip comments, doctype, and other special strings
                if isinstance(element, (Comment, Doctype)):
                    return

                # Only translate if the text is substantial
                if should_translate_text(element.string):
                    try:
                        # Preserve leading/trailing whitespace
                        original = element.string
                        stripped = original.strip()
                        leading_ws = original[:len(original) - len(original.lstrip())]
                        trailing_ws = original[len(original.rstrip()):]

                        # Translate the stripped text
                        translated = translator.translate(stripped)

                        # Restore whitespace
                        element.string.replace_with(leading_ws + translated + trailing_ws)
                    except (RuntimeError, ValueError, AttributeError) as e:
                        # If translation fails for this node, leave it as-is
                        print(f"[translate] Failed to translate text node: {e}", file=sys.stderr)
                        pass
            elif hasattr(element, 'children'):
                # Recursively process all children
                for child in list(element.children):
                    translate_element(child)

        # Translate all text nodes in the document
        translate_element(soup)

        # Return the HTML, preserving the original structure as much as possible
        # Use str() instead of prettify() to avoid reformatting
        return str(soup)

    except (ImportError, ValueError, RuntimeError) as e:
        print(f"[translate] HTML parsing failed: {e}, falling back to plain translation", file=sys.stderr)
        # Fall back to plain text translation
        return translator.translate(html_content)


def translate_offline(text: str, target: str, is_html: bool, install_on_demand: bool = True) -> str:
    """
    Translate text using ArgosTranslate offline translation.

    Args:
        text: Text or HTML to translate
        target: Target language code (ISO 639-1)
        is_html: Whether input is HTML (preserves structure if True)
        install_on_demand: Whether to auto-download missing models (default: True)

    Returns:
        Translated text, or original text if translation fails
    """
    debug_log(f"=== TRANSLATE REQUEST ===")
    debug_log(f"Input length: {len(text)}")
    debug_log(f"Input preview: {text[:200] if len(text) > 200 else text}")
    debug_log(f"Target: {target}, HTML: {is_html}, Install-on-demand: {install_on_demand}")

    fake_mode = os.environ.get("TRANSLATE_FAKE_UPPERCASE") == "1"

    if fake_mode:
        class _FakeTranslator:
            def translate(self, s: str) -> str:
                return s.upper()
        translator = _FakeTranslator()
        if is_html:
            return translate_html_carefully(translator, text)
        return translator.translate(text)

    try:
        import argostranslate.package as argospkg
        import argostranslate.translate as argostrans
        debug_log("Argos modules imported successfully")
    except ImportError as e:
        debug_log(f"Failed to import argos: {e}")
        print(f"[translate] ERROR: ArgosTranslate not available: {e}", file=sys.stderr)
        return text

    # Language detection (optional)
    detected = None
    if not fake_mode:
        try:
            from langdetect import detect
            # For HTML, try to extract text content for better detection
            text_for_detection = text
            if is_html:
                try:
                    from bs4 import BeautifulSoup
                    soup = BeautifulSoup(text, 'html.parser')
                    text_for_detection = soup.get_text()
                except (ImportError, ValueError):
                    pass  # Fall back to full HTML
            detected = detect(text_for_detection)
            debug_log(f"Detected language: {detected}")
        except (ImportError, ValueError, RuntimeError) as e:
            debug_log(f"Language detection failed: {e}")

    from_code = detected or "auto"

    # Fallback: Afrikaans is often misdetected Dutch
    if from_code == "af":
        from_code = "nl"

    # If no language detected, return original text
    if from_code == "auto" or from_code == target:
        debug_log(f"No translation needed (from={from_code}, target={target})")
        return text

    # Try installed languages first
    installed = argostrans.get_installed_languages()
    debug_log(f"Installed languages: {[l.code for l in installed]}")

    src_lang = next((l for l in installed if l.code == from_code), None)
    tgt_lang = next((l for l in installed if l.code == target), None)

    debug_log(f"Source lang: {src_lang.code if src_lang else 'None'}")
    debug_log(f"Target lang: {tgt_lang.code if tgt_lang else 'None'}")

    # If models are missing, try to auto-download (if enabled)
    if not src_lang or not tgt_lang:
        if install_on_demand:
            print(f"[translate] Model {from_code} → {target} not installed, attempting auto-download...", file=sys.stderr)
            debug_log(f"Model {from_code} → {target} not installed, attempting auto-download...")
            if auto_download_model(from_code, target, debug_func=debug_log):
                # Reload installed languages after download
                installed = argostrans.get_installed_languages()
                src_lang = next((l for l in installed if l.code == from_code), None)
                tgt_lang = next((l for l in installed if l.code == target), None)
                debug_log(f"After download - Source lang: {src_lang.code if src_lang else 'None'}")
                debug_log(f"After download - Target lang: {tgt_lang.code if tgt_lang else 'None'}")
        else:
            print(f"[translate] ERROR: Model {from_code} → {target} not installed", file=sys.stderr)
            print(f"[translate] Auto-download is disabled. Please install models manually using setup_models.py", file=sys.stderr)
            debug_log(f"Model {from_code} → {target} not installed, auto-download disabled")
            return text

    if src_lang and tgt_lang:
        try:
            translator = src_lang.get_translation(tgt_lang)
            debug_log(f"Translator found: {translator}")

            # Use our custom HTML translation for HTML content
            if is_html:
                result = translate_html_carefully(translator, text)
                debug_log(f"HTML translation result length: {len(result)}")
                debug_log(f"Translation preview: {result[:200] if len(result) > 200 else result}")
                return result

            # Plain text translation
            result = translator.translate(text)
            debug_log(f"Translation result length: {len(result)}")
            debug_log(f"Translation preview: {result[:200] if len(result) > 200 else result}")
            return result
        except (RuntimeError, ValueError, AttributeError, OSError) as e:
            debug_log(f"Translation failed: {e}")
            print(f"[translate] ERROR: Translation failed: {e}", file=sys.stderr)
            return text

    # Best-effort: no-op if translator cannot be found
    debug_log("No translator found, returning original")
    return text

def main() -> int:
    """Main entry point for the translation runner."""
    global DEBUG_MODE

    ap = argparse.ArgumentParser(description="Offline translation using ArgosTranslate")
    ap.add_argument("--target", default="en", help="Target language (ISO 639-1 code, default: en)")
    ap.add_argument("--html", dest="is_html", action="store_true", help="Input is HTML")
    ap.add_argument("--text", dest="is_html", action="store_false", help="Input is plain text (default)")
    ap.add_argument("--install-on-demand", dest="install_on_demand", action="store_true", default=True,
                    help="Enable automatic download of missing models (default)")
    ap.add_argument("--no-install-on-demand", dest="install_on_demand", action="store_false",
                    help="Disable automatic download of missing models")
    ap.add_argument("--debug", action="store_true", help=f"Enable debug logging to {DEBUG_LOG_FILE}")
    args = ap.parse_args()

    # Enable debug mode if requested
    if args.debug:
        DEBUG_MODE = True
        debug_log("\n\n=== NEW TRANSLATION REQUEST (DEBUG MODE) ===")
        debug_log(f"Args: target={args.target}, html={args.is_html}, install_on_demand={args.install_on_demand}")

    data = sys.stdin.read()
    if args.debug:
        debug_log(f"Read {len(data)} bytes from stdin")

    try:
        out = translate_offline(data, args.target, args.is_html, args.install_on_demand)
        if args.debug:
            debug_log(f"Output length: {len(out)}")
    except Exception as e:
        if args.debug:
            debug_log(f"Exception in main: {e}")
        print(f"[translate] ERROR: Unexpected exception: {e}", file=sys.stderr)
        out = data

    # Output JSON response with translated content
    response = {"translated": out}
    sys.stdout.write(json.dumps(response))
    sys.stdout.flush()

    if args.debug:
        debug_log("=== END ===\n")

    return 0

if __name__ == "__main__":
    sys.exit(main())
