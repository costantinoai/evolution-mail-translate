import importlib

import pytest
from bs4 import BeautifulSoup

class Translator:
    def __init__(self, limit=None, error=None):
        self.calls = []
        self.limit = limit
        self.error = error

    def translate(self, text):
        self.calls.append(text)
        if self.error:
            raise self.error
        if self.limit and len(text) > self.limit:
            raise ValueError("request too long")
        return text.upper()


@pytest.fixture(params=["translate_runner", "translate_runner_online"])
def runner(request):
    return importlib.import_module(request.param)


def test_html_preserves_excluded_ancestors_and_whitespace(runner):
    translator = Translator()
    html = ('<!DOCTYPE html><!--keep--><style>.small { color: red; }</style>'
            '<pre><span>keep code</span></pre><p title="keep">  hello <b>world</b>  </p>')
    result = runner.translate_html_carefully(translator, html)
    soup = BeautifulSoup(result, "html.parser")
    assert soup.style.string == ".small { color: red; }"
    assert soup.pre.get_text() == "keep code"
    assert soup.p["title"] == "keep"
    assert soup.p.get_text() == BeautifulSoup(html, "html.parser").p.get_text().upper()
    assert "<!--keep-->" in result
    assert translator.calls == ["hello", "world"]


def test_html_failures_are_reported(runner):
    with pytest.raises(RuntimeError, match="provider unavailable"):
        runner.translate_html_carefully(Translator(error=RuntimeError("provider unavailable")), "<p>hello</p>")


def test_long_html_text_respects_provider_limit():
    runner = importlib.import_module("translate_runner_online")
    translator = Translator(limit=499)
    html = "<p>" + "word " * 220 + "</p>"
    result = runner.translate_html_carefully(translator, html, max_chars=499)
    assert BeautifulSoup(result, "html.parser").p.get_text() == "WORD " * 220
    assert all(len(text) <= 499 for text in translator.calls)
