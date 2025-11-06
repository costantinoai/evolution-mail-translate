# User Guide

## Overview

Translate email bodies in GNOME Evolution offline using ArgosTranslate. HTML formatting is preserved.

## Install

### From .deb (recommended)

```bash
sudo apt install ./evolution-translate-extension_1.0.0-1_amd64.deb
```

Then run once per user to prepare the Python environment and models:

```bash
evolution-translate-setup
```

Restart Evolution:

```bash
killall evolution 2>/dev/null || true
evolution &
```

### From Source

```bash
sudo apt install cmake pkg-config evolution-dev evolution-data-server-dev \
  python3 python3-venv python3-pip
./scripts/install-from-source.sh
evolution-translate-setup
killall evolution 2>/dev/null || true
evolution &
```

## Usage

- Translate current message: `Ctrl+Shift+T` or Tools → Translate Message
- Toggle to original: `Ctrl+Shift+T` again (or `Ctrl+Shift+O` for “Show Original”)
- Settings: Edit → Preferences → Translate Settings

### Settings Explained

- Target language: default output language for translations
- Install models on demand: automatically download missing Argos Translate models the first time they are needed
- Python setup: run `evolution-translate-setup` once per user to create a virtualenv and install Python dependencies and (optionally) default models

Advanced (optional):
- `TRANSLATE_HELPER_PATH` can point to a local translate_runner.py for development
- `TRANSLATE_PYTHON_BIN` can point to a custom Python interpreter (e.g., inside your venv)

## Notes

- Module path: `/usr/lib*/evolution/modules/libtranslate-module.so`
- Helper scripts: `/usr/share/evolution-translate/translate/`
- Per‑user Python env: `~/.local/lib/evolution-translate/venv`
- Models: `~/.local/share/argos-translate/packages/`
