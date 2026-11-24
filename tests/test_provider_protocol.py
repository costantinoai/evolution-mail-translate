"""Exercise the public C provider API with a controlled subprocess, not a network mock."""
import json
import os
from pathlib import Path
import shlex
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]
MODULE = Path(os.environ.get("TRANSLATE_TEST_MODULE", ROOT / "build/src/libtranslate-module.so"))


@pytest.fixture(scope="module")
def probe(tmp_path_factory):
    directory = tmp_path_factory.mktemp("provider-probe")
    executable = directory / "probe"
    flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "gio-2.0", "gmodule-2.0"], text=True))
    subprocess.run(["gcc", "-Wall", "-Wextra", str(ROOT / "tests/provider_probe.c"),
                    "-o", str(executable), *flags], check=True, capture_output=True, text=True)
    helper = directory / "helper.py"
    helper.write_text(
        "import os, sys\nsys.stdin.read()\n"
        "response = os.environ['PROBE_RESPONSE']\n"
        "if response == 'fallback':\n"
        "    response = '{\"translated\":\"offline result\"}' if '--no-install-on-demand' in sys.argv else '{\"error\":\"Google HTTP 429\"}'\n"
        "print(response)\n")
    schemas = directory / "schemas"
    schemas.mkdir()
    (schemas / "org.gnome.evolution.translate.gschema.xml").write_bytes(
        (ROOT / "data/gschema/org.gnome.evolution.translate.gschema.xml").read_bytes())
    subprocess.run(["glib-compile-schemas", str(schemas)], check=True, capture_output=True, text=True)
    return executable, helper, schemas


def run_probe(probe, provider, response):
    executable, helper, schemas = probe
    environment = dict(os.environ, LD_LIBRARY_PATH="/usr/lib/evolution",
                       GSETTINGS_BACKEND="memory", GSETTINGS_SCHEMA_DIR=str(schemas),
                       TRANSLATE_PYTHON_BIN="/usr/bin/python3",
                       TRANSLATE_HELPER_PATH=str(helper), PROBE_RESPONSE=response)
    return subprocess.run([str(executable), str(MODULE), provider],
                          env=environment, text=True, capture_output=True, timeout=10)


@pytest.mark.parametrize("provider", ["google", "mymemory", "libre", "argos"])
@pytest.mark.parametrize("response", [
    '{"error":"service unavailable","translated":"original"}',
    'this is not JSON', '{"translated":123}', '{"translated":null}',
    '{}', '[]', '',
])
def test_invalid_response_is_an_error(probe, provider, response):
    result = run_probe(probe, provider, response)
    assert result.returncode == 0, result.stderr
    assert "callback: success=0; error=" in result.stdout, result.stdout
    assert "error=none" not in result.stdout
    assert "CRITICAL" not in result.stderr


@pytest.mark.parametrize("provider", ["google", "mymemory", "libre", "argos"])
def test_valid_response_succeeds(probe, provider):
    result = run_probe(probe, provider, json.dumps({"translated": "<p>Hello, caffè.</p>"}))
    assert result.returncode == 0, result.stderr
    assert "callback: success=1; error=none" in result.stdout


def test_unknown_provider_completes_with_error(probe):
    result = run_probe(probe, "unknown-common", '{}')
    assert result.returncode == 0, result.stdout + result.stderr
    assert "Unknown translation provider" in result.stdout


def test_online_failure_uses_installed_offline_model_and_reports_it(probe):
    result = run_probe(probe, "common", "fallback")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "success=1" in result.stdout
    assert "translation: offline result" in result.stdout
    assert "fallback notice: Google HTTP 429" in result.stdout


def test_both_failures_preserve_online_error(probe):
    result = run_probe(probe, "common", '{"error":"Google HTTP 429"}')
    assert result.returncode == 0, result.stdout + result.stderr
    assert "success=0" in result.stdout
    assert "Google HTTP 429" in result.stdout
    assert "Offline fallback also failed" in result.stdout
