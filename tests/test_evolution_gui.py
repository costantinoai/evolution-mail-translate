"""Click the extension's actions in the real Evolution executable on private Xvfb."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
MODULE = Path(os.environ.get("TRANSLATE_TEST_MODULE", ROOT / "build/src/libtranslate-module.so"))


@pytest.fixture(scope="module")
def gui_driver(tmp_path_factory):
    directory = tmp_path_factory.mktemp("evolution-gui-driver")
    driver = directory / "driver.so"
    flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "evolution-mail-3.0", "gmodule-2.0"], text=True))
    subprocess.run(["gcc", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror",
                    "-Wno-deprecated-declarations", str(ROOT / "tests/evolution_driver.c"),
                    "-o", str(driver), *flags, "-ldl"], check=True, capture_output=True, text=True)
    modules = directory / "modules"
    modules.mkdir()
    for source in Path("/usr/lib/evolution/modules").glob("*.so"):
        (modules / source.name).symlink_to(MODULE if source.name == "libtranslate-module.so" else source)
    if not (modules / "libtranslate-module.so").exists():
        (modules / "libtranslate-module.so").symlink_to(MODULE)
    helper = directory / "helper.py"
    helper.write_text(
        "import json, os, sys, time\n"
        "data=sys.stdin.read()\ntime.sleep(1)\n"
        "mode=os.environ.get('TRANSLATE_GUI_AUDIT')\n"
        "result={'translated':data.replace('Goedemorgen, dit is een testbericht.', 'GOEDEMORGEN, DIT IS EEN TESTBERICHT.')}\n"
        "if mode in ('error','fallback') and '--provider' in sys.argv: result={'error':'Google is rate-limiting translation (HTTP 429).'}\n"
        "elif mode=='error': result={'error':'No installed Argos model.'}\n"
        "print(json.dumps(result))\n")
    schemas = directory / "schemas"
    schemas.mkdir()
    (schemas / "org.gnome.evolution.translate.gschema.xml").write_bytes(
        (ROOT / "data/gschema/org.gnome.evolution.translate.gschema.xml").read_bytes())
    subprocess.run(["glib-compile-schemas", str(schemas)], check=True, capture_output=True, text=True)
    return driver, modules, helper, schemas


@pytest.mark.parametrize("mode", ["preview", "browser", "stale", "destroy", "error", "fallback", "settings"])
def test_real_evolution_actions(gui_driver, tmp_path, mode):
    driver, modules, helper, schemas = gui_driver
    runtime_context = tempfile.TemporaryDirectory(prefix="evolution-translate-test-",
                                                 dir=Path("/run/user") / str(os.getuid()))
    runtime = Path(runtime_context.name)
    environment = dict(os.environ,
        XDG_CONFIG_HOME=str(tmp_path / "config"), XDG_DATA_HOME=str(tmp_path / "data"),
        XDG_CACHE_HOME=str(tmp_path / "cache"), XDG_RUNTIME_DIR=str(runtime),
        GSETTINGS_BACKEND="memory", GSETTINGS_SCHEMA_DIR=str(schemas),
        LD_PRELOAD=str(driver), LD_LIBRARY_PATH="/usr/lib/evolution",
        TRANSLATE_GUI_AUDIT=mode, TRANSLATE_GUI_MODULE_DIR=str(modules),
        TRANSLATE_GUI_MODULE=str(MODULE), TRANSLATE_GUI_EVIDENCE=str(tmp_path),
        TRANSLATE_PYTHON_BIN="/usr/bin/python3", TRANSLATE_HELPER_PATH=str(helper),
        NO_AT_BRIDGE="1", LIBGL_ALWAYS_SOFTWARE="1", LC_ALL="C.UTF-8")
    if os.environ.get("TRANSLATE_GUI_USE_MODELS") == "1":
        environment.update(
            TRANSLATE_PYTHON_BIN=str(Path.home() / ".local/lib/evolution-translate/venv/bin/python"),
            TRANSLATE_HELPER_PATH=str(ROOT / "tools/translate/translate_runner.py"),
            ARGOS_PACKAGES_DIR=str(Path.home() / ".local/share/argos-translate/packages"),
            ARGOS_DEVICE_TYPE="cpu")
        if os.environ.get("TRANSLATE_GUI_USE_INSTALLED_HELPERS") == "1":
            environment.pop("TRANSLATE_HELPER_PATH", None)
            environment.pop("TRANSLATE_PYTHON_BIN", None)
            environment.pop("GSETTINGS_SCHEMA_DIR", None)
    # Preload only Evolution, never xvfb-run, the bus daemon or the user's session.
    environment.pop("LD_PRELOAD")
    result = subprocess.run(["xvfb-run", "-a", "dbus-run-session", "--", "env",
                             "LD_PRELOAD=" + str(driver), "evolution", "--offline"],
                            env=environment, capture_output=True, text=True, timeout=75)
    log = result.stdout + result.stderr
    (tmp_path / "evolution.txt").write_text(log)
    if os.environ.get("TRANSLATE_GUI_EVIDENCE_DIR"):
        destination = Path(os.environ["TRANSLATE_GUI_EVIDENCE_DIR"])
        destination.mkdir(parents=True, exist_ok=True)
        (destination / f"gui-{mode}.txt").write_text(log)
        for screenshot in tmp_path.glob("*.png"):
            (destination / screenshot.name).write_bytes(screenshot.read_bytes())
    assert result.returncode == 0, log[-8000:]
    assert f"GUI {mode}: PASS" in result.stdout, log[-8000:]
