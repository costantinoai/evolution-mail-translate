#!/usr/bin/env bash
# Build in an isolated source snapshot; never delete neighbouring packages.
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
for tool in git python3 dpkg-buildpackage dh cmake pkg-config lintian; do
  command -v "$tool" >/dev/null || { echo "Missing build tool: $tool" >&2; exit 1; }
done
mkdir -p "$PROJECT_ROOT/build/debian" "$PROJECT_ROOT/packages"
RUN_DIR="$(mktemp -d "$PROJECT_ROOT/build/debian/run.XXXXXX")"
SNAPSHOT="$RUN_DIR/source"
python3 - "$PROJECT_ROOT" "$SNAPSHOT" <<'COPY'
from pathlib import Path
import shutil
import subprocess
import sys
root, destination = map(Path, sys.argv[1:])
files = subprocess.check_output([
    "git", "-C", str(root), "ls-files", "-z", "--cached", "--others", "--exclude-standard",
    "--", ".", ":(exclude)tasks/**",
]).decode().split("\0")
for name in filter(None, files):
    source, target = root / name, destination / name
    if not source.exists() and not source.is_symlink():
        continue
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.is_symlink():
        target.symlink_to(source.readlink())
    else:
        shutil.copy2(source, target)
COPY
printf 'Building snapshot: %s\n' "$SNAPSHOT"
(cd "$SNAPSHOT"; dpkg-buildpackage -us -uc -b -j2)
shopt -s nullglob
artifacts=("$RUN_DIR"/*.deb "$RUN_DIR"/*.changes "$RUN_DIR"/*.buildinfo)
if (( ${#artifacts[@]} == 0 )); then
  echo "The build produced no package artifacts." >&2
  exit 1
fi
for artifact in "${artifacts[@]}"; do
  cp -p -- "$artifact" "$PROJECT_ROOT/packages/"
done
for package in "$RUN_DIR"/*.deb; do
  lintian --fail-on error "$package"
done
printf 'Packages copied to %s/packages; build snapshot retained in %s\n' "$PROJECT_ROOT" "$RUN_DIR"
