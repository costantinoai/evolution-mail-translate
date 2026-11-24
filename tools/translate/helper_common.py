"""Shared HTML handling and the helper JSON protocol."""
import html
import json
import logging
import re
import sys

SKIP_TAGS = frozenset({
    "head", "style", "script", "code", "pre", "kbd", "samp", "var", "svg",
    "math", "textarea", "noscript",
})
ADDRESS = re.compile(r"^(?:https?://|mailto:)\S+$|^[^\s@]+@[^\s@]+\.[^\s@]+$", re.I)


class TranslationError(RuntimeError):
    """An actionable failure that can be shown without including message text."""


def detect_language(text, is_html=False):
    from langdetect import DetectorFactory, LangDetectException, detect

    DetectorFactory.seed = 0
    if is_html:
        from bs4 import BeautifulSoup
        soup = BeautifulSoup(text, "html.parser")
        for element in soup.find_all(list(SKIP_TAGS)):
            element.decompose()
        text = soup.get_text(" ", strip=True)
    try:
        return detect(text)
    except LangDetectException as error:
        raise TranslationError("Could not detect the message language. The message may contain too little text.") from error


def translate_text(translator, text, max_chars=None):
    """Translate bounded chunks while retaining whitespace at their boundaries."""
    chunks = []
    remaining = text
    while max_chars and len(remaining) > max_chars:
        boundaries = list(re.finditer(r"\s+", remaining[:max_chars]))
        cut = boundaries[-1].end() if boundaries else max_chars
        chunks.append(remaining[:cut])
        remaining = remaining[cut:]
    chunks.append(remaining)
    output = []
    for chunk in chunks:
        stripped = chunk.strip()
        if not stripped:
            output.append(chunk)
            continue
        translated = translator.translate(stripped)
        if not isinstance(translated, str) or not translated.strip():
            raise TranslationError("The translation provider returned an empty or invalid result.")
        leading = chunk[:len(chunk) - len(chunk.lstrip())]
        trailing = chunk[len(chunk.rstrip()):]
        output.append(leading + translated.strip() + trailing)
    return "".join(output)


def translate_html_carefully(translator, html_content, max_chars=None):
    """Translate text nodes; skip excluded ancestors and keep errors visible."""
    from bs4 import BeautifulSoup, Comment, Doctype, NavigableString
    soup = BeautifulSoup(html_content, "html.parser")
    for node in list(soup.find_all(string=True)):
        if isinstance(node, (Comment, Doctype)):
            continue
        text = str(node)
        if not any(character.isalpha() for character in text) or ADDRESS.fullmatch(text.strip()):
            continue
        if any(parent.name in SKIP_TAGS or parent.get("translate", "").lower() == "no"
               or "notranslate" in parent.get("class", []) for parent in node.parents):
            continue
        node.replace_with(NavigableString(translate_text(translator, text, max_chars)))
    return str(soup)


def translate_content(translator, text, is_html, preserve_format=True, max_chars=None):
    if is_html and preserve_format:
        return translate_html_carefully(translator, text, max_chars)
    if is_html:
        from bs4 import BeautifulSoup
        soup = BeautifulSoup(text, "html.parser")
        for element in soup.find_all(list(SKIP_TAGS)):
            element.decompose()
        result = translate_text(translator, soup.get_text("\n"), max_chars)
        return html.escape(result).replace("\n", "<br>")
    return translate_text(translator, text, max_chars)


def write_result(result):
    # stdout is exclusively a UTF-8 JSON response. Diagnostics go to stderr.
    sys.stdout.write(json.dumps(result, ensure_ascii=False) + "\n")
    sys.stdout.flush()
    return 1 if "error" in result else 0


def configure_logging(debug):
    logging.basicConfig(level=logging.DEBUG if debug else logging.WARNING,
                        format="[translate] %(message)s", stream=sys.stderr)
