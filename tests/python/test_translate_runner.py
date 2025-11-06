import importlib.util
import sys
from pathlib import Path


def _load_runner():
    root = Path(__file__).resolve().parents[2]
    runner_path = root / "tools" / "translate" / "translate_runner.py"
    spec = importlib.util.spec_from_file_location("translate_runner", str(runner_path))
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def test_returns_original_text_when_no_deps():
    runner = _load_runner()
    out = runner.translate_offline("Bonjour", target="en", is_html=False, install_on_demand=False)
    assert out == "Bonjour"


def test_html_preservation_with_mock(monkeypatch):
    # Mock langdetect
    class _LD:
        @staticmethod
        def detect(_):
            return "es"

    monkeypatch.setitem(sys.modules, "langdetect", _LD)

    # Mock argostranslate.translate
    class MockTranslator:
        def translate(self, text):
            return text.upper()

    class MockLang:
        def __init__(self, code):
            self.code = code

        def get_translation(self, to_lang):
            return MockTranslator()

    class ArgosTranslate:
        @staticmethod
        def get_installed_languages():
            return [MockLang("es"), MockLang("en")]

    monkeypatch.setitem(sys.modules, "argostranslate.translate", ArgosTranslate)

    # Mock argostranslate.package used by auto_download_model (won't be called)
    class ArgosPkg:
        @staticmethod
        def update_package_index():
            return None

        @staticmethod
        def get_available_packages():
            return []

    monkeypatch.setitem(sys.modules, "argostranslate.package", ArgosPkg)

    runner = _load_runner()
    html_in = "<p>hola <b>mundo</b>!</p>"
    out = runner.translate_offline(html_in, target="en", is_html=True, install_on_demand=False)
    assert out == "<p>HOLA <b>MUNDO</b>!</p>"

