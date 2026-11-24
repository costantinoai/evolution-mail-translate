import importlib
import sys
import types

import pytest


@pytest.fixture
def online(monkeypatch):
    runner = importlib.import_module("translate_runner_online")
    library = types.ModuleType("deep_translator")
    exceptions = types.ModuleType("deep_translator.exceptions")
    exceptions.RequestError = type("RequestError", (Exception,), {})
    exceptions.TooManyRequests = type("TooManyRequests", (Exception,), {})
    constants = types.ModuleType("deep_translator.constants")
    constants.MY_MEMORY_LANGUAGES_TO_CODES = {"dutch": "nl-NL", "english": "en-GB"}
    requests = types.ModuleType("requests")
    requests.Timeout = type("Timeout", (Exception,), {})
    requests.ConnectionError = type("ConnectionError", (Exception,), {})
    for name, module in [("deep_translator", library), ("deep_translator.exceptions", exceptions),
                         ("deep_translator.constants", constants), ("requests", requests)]:
        monkeypatch.setitem(sys.modules, name, module)
    return runner, library, exceptions


def test_google_rate_limit_is_explicit_and_not_retried(online, monkeypatch):
    runner, library, exceptions = online
    calls = []

    class LimitedTranslator:
        def __init__(self, **kwargs):
            pass

        def translate(self, text):
            calls.append(text)
            raise exceptions.TooManyRequests()

    library.GoogleTranslator = library.MyMemoryTranslator = LimitedTranslator
    monkeypatch.setattr(runner.time, "sleep", lambda _: pytest.fail("HTTP 429 must not be retried"))
    result = runner.translate_online("<p>hello</p>", "en", "google", True)
    assert "HTTP 429" in result["error"]
    assert "translated" not in result
    assert len(calls) == 1


def test_mymemory_html_uses_bounded_requests_and_provider_language_codes(online):
    runner, library, _ = online
    calls = []

    class BoundedTranslator:
        def __init__(self, source, target):
            assert (source, target) == ("nl-NL", "en-GB")

        def translate(self, text):
            assert len(text) < 500
            calls.append(text)
            return text.upper()

    library.GoogleTranslator = library.MyMemoryTranslator = BoundedTranslator
    result = runner.translate_online("<p>" + "woord " * 220 + "</p>", "en", "mymemory", True, source_lang="nl")
    assert "error" not in result
    assert "WOORD " * 220 in result["translated"]
    assert len(calls) > 1


def test_html_network_failures_reach_retry_logic(online, monkeypatch):
    runner, library, exceptions = online
    calls, delays = [], []

    class IntermittentTranslator:
        def __init__(self, **kwargs):
            pass

        def translate(self, text):
            calls.append(text)
            if len(calls) < 3:
                raise exceptions.RequestError()
            return "translated"

    library.GoogleTranslator = library.MyMemoryTranslator = IntermittentTranslator
    monkeypatch.setattr(runner.time, "sleep", delays.append)
    result = runner.translate_online("<p>hello</p>", "en", "google", True)
    assert result == {"translated": "<p>translated</p>"}
    assert delays == [1, 2]
