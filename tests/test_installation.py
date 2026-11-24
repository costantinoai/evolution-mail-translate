import os
from pathlib import Path
import stat
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("TRANSLATE_TEST_BUILD",
    Path(os.environ.get("TRANSLATE_TEST_MODULE", ROOT / "build/src/libtranslate-module.so")).parent.parent))


def test_install_and_uninstall_are_scoped_and_manifest_is_complete(tmp_path):
    sentinel = tmp_path / "usr/share/unrelated-file"
    sentinel.parent.mkdir(parents=True)
    sentinel.write_text("keep these permissions")
    sentinel.chmod(0o600)
    environment = dict(os.environ, DESTDIR=str(tmp_path))
    result = subprocess.run(["cmake", "--install", str(BUILD)], env=environment,
                            text=True, capture_output=True)
    assert result.returncode == 0, result.stdout + result.stderr
    manifest = (BUILD / "install_manifest.txt").read_text().splitlines()
    assert manifest
    assert all((tmp_path / path.lstrip("/")).is_file() for path in manifest)
    assert any(path.endswith("evolution-translate-setup.1.gz") for path in manifest)
    assert any(path.endswith("requirements-online.txt") for path in manifest)
    assert stat.S_IMODE(sentinel.stat().st_mode) == 0o600
    result = subprocess.run(["cmake", "-P", str(BUILD / "cmake_uninstall.cmake")],
                            env=environment, text=True, capture_output=True)
    assert result.returncode == 0, result.stdout + result.stderr
    assert all(not (tmp_path / path.lstrip("/")).exists() for path in manifest)
    assert sentinel.read_text() == "keep these permissions"
    assert stat.S_IMODE(sentinel.stat().st_mode) == 0o600
