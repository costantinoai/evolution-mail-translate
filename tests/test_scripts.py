from pathlib import Path
import shutil
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("version", ["1.2.3", "1.2.4"])
def test_release_preview_preserves_tree_and_uses_exact_patch_tag(tmp_path, version):
    (tmp_path / "scripts").mkdir()
    script = tmp_path / "scripts/prepare-release.sh"
    shutil.copy2(ROOT / "scripts/prepare-release.sh", script)
    (tmp_path / "debian").mkdir()
    changelog = tmp_path / "debian/changelog"
    changelog.write_bytes((ROOT / "debian/changelog").read_bytes())
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True, capture_output=True)
    before = changelog.read_bytes()
    result = subprocess.run(["bash", str(script), version, "--dry-run"], text=True, capture_output=True)
    assert result.returncode == 0, result.stderr
    assert f"Tag: v{version}\n" in result.stdout
    assert "No push requested." in result.stdout
    assert changelog.read_bytes() == before
    assert not subprocess.check_output(["git", "-C", str(tmp_path), "tag", "--list"])


def test_shell_scripts_parse_individually():
    for script in sorted((ROOT / "scripts").glob("*.sh")):
        result = subprocess.run(["bash", "-n", str(script)], capture_output=True, text=True)
        assert result.returncode == 0, str(script) + result.stderr


def test_release_preparation_creates_matching_package_version_and_tag(tmp_path):
    (tmp_path / "scripts").mkdir()
    script = tmp_path / "scripts/prepare-release.sh"
    shutil.copy2(ROOT / "scripts/prepare-release.sh", script)
    (tmp_path / "debian").mkdir()
    (tmp_path / "debian/changelog").write_bytes((ROOT / "debian/changelog").read_bytes())
    git = ["git", "-C", str(tmp_path)]
    subprocess.run([*git, "init", "-q"], check=True, capture_output=True)
    subprocess.run([*git, "config", "user.name", "Release Test"], check=True)
    subprocess.run([*git, "config", "user.email", "release@example.invalid"], check=True)
    subprocess.run([*git, "add", "--", "scripts/prepare-release.sh", "debian/changelog"], check=True)
    subprocess.run([*git, "commit", "-qm", "Initial fixture"], check=True, capture_output=True)
    result = subprocess.run(["bash", str(script), "9.9.9"], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    for field, expected in [("Version", "9.9.9-1"), ("Distribution", "unstable")]:
        assert subprocess.check_output(["dpkg-parsechangelog", "-S", field],
                                       cwd=tmp_path, text=True).strip() == expected
    assert subprocess.check_output([*git, "rev-parse", "v9.9.9^{}"], text=True) == \
        subprocess.check_output([*git, "rev-parse", "HEAD"], text=True)
    assert not subprocess.check_output([*git, "status", "--porcelain"])
    assert "Nothing was pushed." in result.stdout


def test_setup_help_does_not_create_environment():
    result = subprocess.run(["bash", str(ROOT / "scripts/evolution-translate-setup.sh"), "--help"],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert "--online-only" in result.stdout
