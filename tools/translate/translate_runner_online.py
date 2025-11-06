#!/usr/bin/env python3
"""
translate_runner_online.py
Online translation runner using deep-translator library.
Reads HTML/text from stdin and writes translated content to stdout as JSON.

Options:
  --target <lang>     target ISO 639-1 language code (default: en)
  --provider <name>   translation provider (google, mymemory, libre, etc.)
  --html | --text     hint whether input is HTML (default: text)
  --debug             enable debug logging to /tmp/translate_online_debug.log

Supported providers:
  - google: Google Translate (free, no API key)
  - mymemory: MyMemory Translator (free, no API key, 500 chars/request limit)
  - libre: LibreTranslate (free, no API key if using public instance)
  - microsoft: Microsoft Translator (requires API key)
  - deepl: DeepL Translator (requires API key)
"""

import argparse
import os
import sys
import json
from typing import Optional

# Debug logging support
DEBUG_MODE = False
DEBUG_LOG_FILE = "/tmp/translate_online_debug.log"

def debug_log(msg):
    """Log debug message if debug mode is enabled"""
    if DEBUG_MODE:
        try:
            with open(DEBUG_LOG_FILE, "a") as f:
                f.write(msg + "\n")
        except Exception:
            pass


def detect_language(text: str, is_html: bool = False) -> str:
    """
    Detect the source language of the text.

    Args:
        text: Text to detect language from
        is_html: Whether text is HTML

    Returns:
        ISO 639-1 language code or "auto"
    """
    try:
        from langdetect import detect
        from bs4 import BeautifulSoup

        # Extract text from HTML if needed
        if is_html:
            soup = BeautifulSoup(text, 'html.parser')
            text = soup.get_text()

        # Detect language
        lang_code = detect(text)
        debug_log(f"Detected language: {lang_code}")
        return lang_code
    except Exception as e:
        debug_log(f"Language detection failed: {e}")
        return "auto"


def translate_html_carefully(translator, html_content: str) -> str:
    """
    Translate HTML content while preserving structure.
    Uses BeautifulSoup to parse and only translate text nodes.

    Args:
        translator: deep-translator translator instance
        html_content: HTML content to translate

    Returns:
        Translated HTML with preserved structure
    """
    try:
        from bs4 import BeautifulSoup, NavigableString, Comment, Doctype

        soup = BeautifulSoup(html_content, 'html.parser')

        def translate_node(node):
            """Recursively translate text nodes in the tree"""
            if isinstance(node, NavigableString):
                if isinstance(node, (Comment, Doctype)):
                    return

                text = str(node)
                # Skip empty or whitespace-only text
                if not text or text.isspace():
                    return

                # Skip very short text (likely structural)
                if len(text.strip()) < 2:
                    return

                # Skip numbers-only text
                if text.strip().replace(',', '').replace('.', '').isdigit():
                    return

                # Translate the text
                try:
                    # Preserve leading/trailing whitespace
                    leading_space = text[:len(text) - len(text.lstrip())]
                    trailing_space = text[len(text.rstrip()):]
                    stripped = text.strip()

                    if stripped:
                        translated = translator.translate(stripped)
                        node.replace_with(leading_space + translated + trailing_space)
                        debug_log(f"Translated: '{stripped}' -> '{translated}'")
                except Exception as e:
                    debug_log(f"Failed to translate node: {e}")
            else:
                # Recurse into child nodes
                for child in list(node.children):
                    translate_node(child)

        # Start translation from root
        translate_node(soup)

        return str(soup)
    except Exception as e:
        debug_log(f"HTML translation failed: {e}")
        # Fallback: translate as plain text
        return translator.translate(html_content)


def translate_online(
    text: str,
    target_lang: str,
    provider: str,
    is_html: bool = False,
    api_key: Optional[str] = None
) -> dict:
    """
    Translate text using the specified online provider via deep-translator.

    Args:
        text: Text to translate
        target_lang: Target language code
        provider: Provider name (google, mymemory, libre, etc.)
        is_html: Whether input is HTML
        api_key: Optional API key for providers that require it

    Returns:
        Dict with "translated" key containing translated text
    """
    try:
        from deep_translator import GoogleTranslator, MyMemoryTranslator, LibreTranslator

        debug_log(f"Translating with provider: {provider}")
        debug_log(f"Target language: {target_lang}")
        debug_log(f"Is HTML: {is_html}")
        debug_log(f"Input length: {len(text)} chars")

        # Detect source language
        source_lang = detect_language(text, is_html)
        if source_lang == target_lang:
            debug_log("Source and target languages are the same, returning input")
            return {"translated": text}

        # Create appropriate translator instance
        translator = None

        if provider == "google":
            translator = GoogleTranslator(source=source_lang, target=target_lang)
        elif provider == "mymemory":
            translator = MyMemoryTranslator(source=source_lang, target=target_lang)
        elif provider == "libre":
            # Use public LibreTranslate instance or custom if specified
            base_url = os.environ.get("LIBRE_TRANSLATE_URL", "https://libretranslate.com")
            translator = LibreTranslator(source=source_lang, target=target_lang, base_url=base_url, api_key=api_key)
        else:
            raise ValueError(f"Unsupported provider: {provider}")

        debug_log(f"Translator created: {translator}")

        # Translate based on content type
        if is_html:
            translated = translate_html_carefully(translator, text)
        else:
            # For plain text, split into chunks if needed (some providers have limits)
            if provider == "mymemory" and len(text) > 500:
                # MyMemory has 500 char limit, split into sentences
                sentences = text.split('. ')
                translated_sentences = []
                for sentence in sentences:
                    if sentence:
                        translated_sentences.append(translator.translate(sentence))
                translated = '. '.join(translated_sentences)
            else:
                translated = translator.translate(text)

        debug_log(f"Translation successful, output length: {len(translated)} chars")
        return {"translated": translated}

    except ImportError as e:
        debug_log(f"Import error: {e}")
        return {"error": f"deep-translator not available: {e}", "translated": text}
    except Exception as e:
        debug_log(f"Translation error: {e}")
        return {"error": str(e), "translated": text}


def main():
    """Main entry point for the online translation runner"""
    global DEBUG_MODE

    parser = argparse.ArgumentParser(description="Online translation runner using deep-translator")
    parser.add_argument("--target", default="en", help="Target language code (ISO 639-1)")
    parser.add_argument("--provider", default="google", help="Translation provider (google, mymemory, libre)")
    parser.add_argument("--html", dest="is_html", action="store_true", help="Input is HTML")
    parser.add_argument("--text", dest="is_html", action="store_false", help="Input is plain text")
    parser.add_argument("--debug", action="store_true", help="Enable debug logging")
    parser.add_argument("--api-key", help="API key for providers that require it")
    parser.set_defaults(is_html=False)

    args = parser.parse_args()

    DEBUG_MODE = args.debug

    if DEBUG_MODE:
        debug_log("=" * 60)
        debug_log(f"Online translation runner started")
        debug_log(f"Provider: {args.provider}")
        debug_log(f"Target: {args.target}")
        debug_log(f"HTML mode: {args.is_html}")

    # Read input from stdin
    input_text = sys.stdin.read()

    if not input_text:
        print(json.dumps({"error": "No input provided", "translated": ""}))
        return 1

    # Perform translation
    result = translate_online(
        text=input_text,
        target_lang=args.target,
        provider=args.provider,
        is_html=args.is_html,
        api_key=args.api_key
    )

    # Output result as JSON
    print(json.dumps(result))

    if DEBUG_MODE:
        debug_log("Translation complete")
        debug_log("=" * 60)

    return 0


if __name__ == "__main__":
    sys.exit(main())
