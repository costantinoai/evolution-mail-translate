#!/usr/bin/env bash
# Create the per-user Python environment and install its declared dependencies.
set -euo pipefail

APP_DIR="$HOME/.local/lib/evolution-translate"
VENV_DIR="$APP_DIR/venv"
REQ_FILE="requirements.txt"
MODELS_CHOICE=""
ONLINE_ONLY=false
for arg in "$@"; do
  case "$arg" in
    --models) MODELS_CHOICE=yes ;;
    --no-models) MODELS_CHOICE=no ;;
    --online-only) ONLINE_ONLY=true ;;
    --help|-h)
      echo "Usage: evolution-translate-setup [--online-only] [--models|--no-models]"
      echo "--online-only installs online providers without the optional Argos engine."
      exit 0 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done
if "$ONLINE_ONLY"; then
  if [[ "$MODELS_CHOICE" == yes ]]; then
    echo "--models requires the Argos engine; omit --online-only." >&2
    exit 2
  fi
  REQ_FILE="requirements-online.txt"
  MODELS_CHOICE=no
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
if [[ -f "$ROOT_DIR/$REQ_FILE" ]]; then
  REQ_PATH="$ROOT_DIR/$REQ_FILE"
elif [[ -f "$ROOT_DIR/share/evolution-translate/$REQ_FILE" ]]; then
  REQ_PATH="$ROOT_DIR/share/evolution-translate/$REQ_FILE"
else
  echo "Dependency manifest $REQ_FILE is missing. Reinstall the extension." >&2
  exit 1
fi

command -v python3 >/dev/null
printf 'Evolution Translate setup\nPython environment: %s\nDependencies: %s\n' "$VENV_DIR" "$REQ_PATH"
if [[ ! -x "$VENV_DIR/bin/python" ]]; then
  python3 -m venv "$VENV_DIR"
fi
"$VENV_DIR/bin/python" -m pip install --disable-pip-version-check -r "$REQ_PATH"
"$VENV_DIR/bin/python" -c 'import bs4, langdetect, deep_translator'

if [[ -z "$MODELS_CHOICE" ]]; then
  REPLY=""
  read -r -p "Install default Argos translation models now? [y/N] " REPLY || true
  if [[ "$REPLY" =~ ^[Yy]$ ]]; then MODELS_CHOICE=yes; else MODELS_CHOICE=no; fi
fi
if [[ "$MODELS_CHOICE" == yes ]]; then
  HELPER=""
  for candidate in "$ROOT_DIR/tools/translate/install_default_models.py" \
      "$ROOT_DIR/share/evolution-translate/translate/install_default_models.py" \
      "$APP_DIR/translate/install_default_models.py"; do
    if [[ -f "$candidate" ]]; then HELPER="$candidate"; break; fi
  done
  if [[ -z "$HELPER" ]]; then
    echo "Model installer is missing. Reinstall the extension." >&2
    exit 1
  fi
  "$VENV_DIR/bin/python" "$HELPER"
fi
printf '\nSetup complete. Evolution detects %s automatically.\n' "$VENV_DIR"
