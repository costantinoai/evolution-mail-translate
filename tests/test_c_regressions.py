"""Compile small C regression drivers against the installed Evolution libraries."""
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def test_mime_body_selection(tmp_path):
    flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "evolution-mail-3.0"], text=True))
    executable = tmp_path / "mime-test"
    result = subprocess.run(["gcc", "-Wno-deprecated-declarations",
                             str(ROOT / "tests/test_mime.c"), "-o", str(executable), *flags],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True,
                            env=dict(os.environ, LD_LIBRARY_PATH="/usr/lib/evolution"))
    assert result.returncode == 0, result.stdout + result.stderr
