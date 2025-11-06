#!/usr/bin/env bash
# Clean local build artifacts and Python caches. Leaves virtualenvs intact.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Cleaning build artifacts and Python caches under: $ROOT_DIR"

# Remove CMake build directories
if [ -d "$ROOT_DIR/build" ]; then
  echo "- Removing build/"
  rm -rf "$ROOT_DIR/build"
fi

if [ -d "$ROOT_DIR/_build" ]; then
  echo "- Removing _build/"
  rm -rf "$ROOT_DIR/_build"
fi

# Remove Python caches
echo "- Removing __pycache__ and *.pyc"
find "$ROOT_DIR" -type d -name '__pycache__' -prune -exec rm -rf {} + 2>/dev/null || true
find "$ROOT_DIR" -type f -name '*.pyc' -delete 2>/dev/null || true

echo "Done."

