# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 Andrea Costantino. Added on 2026-11-24.
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

import gpu_utils

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("value", ["auto", "default", "int8", "int8_float32", "float32"])
def test_valid_compute_type_is_preserved(monkeypatch, value):
    monkeypatch.setenv("ARGOS_COMPUTE_TYPE", value)
    assert gpu_utils.setup_compute_type() == value
    assert os.environ["ARGOS_COMPUTE_TYPE"] == value


@pytest.mark.parametrize("value", ["", "not-a-compute-type"])
def test_invalid_compute_type_uses_argos_default(monkeypatch, value):
    monkeypatch.setenv("ARGOS_COMPUTE_TYPE", value)
    assert gpu_utils.setup_compute_type() is None
    assert "ARGOS_COMPUTE_TYPE" not in os.environ


def test_unset_compute_type_keeps_argos_default(monkeypatch):
    monkeypatch.delenv("ARGOS_COMPUTE_TYPE", raising=False)
    assert gpu_utils.setup_compute_type() is None
    assert "ARGOS_COMPUTE_TYPE" not in os.environ


@pytest.mark.parametrize("value, expected", [("int8", "int8"), ("invalid", None), ("", None)])
def test_runner_validates_before_argos_import(tmp_path, value, expected):
    package = tmp_path / "argostranslate"
    package.mkdir()
    (package / "__init__.py").write_text("")
    (package / "translate.py").write_text(
        "import os\n"
        f"assert os.environ.get('ARGOS_COMPUTE_TYPE') == {expected!r}\n"
        "class Language:\n"
        "    def __init__(self, code): self.code = code\n"
        "    def get_translation(self, other): return self\n"
        "    def translate(self, text): return text.upper()\n"
        "def get_installed_languages(): return [Language('nl'), Language('en')]\n")
    environment = dict(os.environ, PYTHONPATH=str(tmp_path), ARGOS_DEVICE_TYPE="cpu",
                       ARGOS_COMPUTE_TYPE=value)
    environment.pop("TRANSLATE_FAKE_UPPERCASE", None)
    result = subprocess.run([sys.executable, str(ROOT / "tools/translate/translate_runner.py"),
                             "--source", "nl", "--target", "en", "--text", "--no-install-on-demand"],
                            input="hello", env=environment, capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stderr
    assert json.loads(result.stdout) == {"translated": "HELLO"}
