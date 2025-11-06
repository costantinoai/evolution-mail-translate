#!/usr/bin/env bash
# evolution-translate-setup: create a user Python environment and install deps/models
set -euo pipefail

APP_DIR="$HOME/.local/lib/evolution-translate"
VENV_DIR="$APP_DIR/venv"
REQ_FILE="requirements.txt"

echo "Evolution Translate - User Setup"
echo "This will create a Python virtualenv under: $VENV_DIR"
echo "and install Python requirements from the repository requirements.txt."
echo

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 is required." >&2
  exit 1
fi

mkdir -p "$APP_DIR"

if [ ! -x "$VENV_DIR/bin/python" ]; then
  echo "Creating virtual environment..."
  python3 -m venv "$VENV_DIR"
else
  echo "Virtual environment already exists at $VENV_DIR"
fi

"$VENV_DIR/bin/python" -m ensurepip --upgrade >/dev/null 2>&1 || true
"$VENV_DIR/bin/python" -m pip install --upgrade pip

# Locate requirements.txt relative to script if run from installed prefix
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
if [ -f "$ROOT_DIR/$REQ_FILE" ]; then
  REQ_PATH="$ROOT_DIR/$REQ_FILE"
elif [ -f "./$REQ_FILE" ]; then
  REQ_PATH="./$REQ_FILE"
else
  echo "WARNING: requirements.txt not found next to script; using built-in defaults"
  REQ_PATH=""
fi

echo "Installing Python dependencies..."
if [ -n "$REQ_PATH" ]; then
  "$VENV_DIR/bin/python" -m pip install -r "$REQ_PATH"
else
  "$VENV_DIR/bin/python" -m pip install argostranslate beautifulsoup4 langdetect
fi

MODELS_CHOICE=""
for arg in "$@"; do
  case "$arg" in
    --models)
      MODELS_CHOICE="yes" ;;
    --no-models)
      MODELS_CHOICE="no" ;;
  esac
done

if [ -z "$MODELS_CHOICE" ]; then
  echo
  read -p "Install default translation models now? [y/N] " -n 1 -r || true
  echo
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    MODELS_CHOICE="yes"
  else
    MODELS_CHOICE="no"
  fi
fi

if [ "$MODELS_CHOICE" = "yes" ]; then
  # Find the helper script installed under /usr/share
  HELPER="/usr/share/evolution-translate/translate/install_default_models.py"
  if [ ! -f "$HELPER" ]; then
    HELPER_LOCAL="$HOME/.local/lib/evolution-translate/translate/install_default_models.py"
    if [ -f "$HELPER_LOCAL" ]; then
      HELPER="$HELPER_LOCAL"
    else
      echo "Could not find install_default_models.py; skipping model installation"
      HELPER=""
    fi
  fi
  if [ -n "$HELPER" ]; then
    "$VENV_DIR/bin/python" "$HELPER" || true
  fi
fi

echo
echo "Done. To use this environment from Evolution, set:"
echo "  export TRANSLATE_PYTHON_BIN=\"$VENV_DIR/bin/python\""
echo "Or simply rely on the default search path (this venv is auto-detected)."
echo
