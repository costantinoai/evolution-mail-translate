# Evolution Translation Extension

**Offline email translation for GNOME Evolution using ArgosTranslate**

[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](#)
[![License](https://img.shields.io/badge/license-LGPL--2.1%2B-green.svg)](#license)
[![Documentation](https://img.shields.io/badge/docs-complete-brightgreen.svg)](USER_GUIDE.md)

## Overview

This Evolution extension adds instant, privacy-preserving email translation directly within GNOME Evolution. Translate foreign language emails to your preferred language with a single click—all processing happens locally on your machine with no data ever leaving your computer.

## Key Features

- **One-Click Translation**: Translate emails directly in the preview pane or message window
- **100% Offline & Private**: Uses local translation models (ArgosTranslate), no internet required, no data transmitted
- **Install-on-Demand**: Automatically downloads missing translation models as needed
- **HTML Email Support**: Preserves formatting, styles, and structure in translated emails
- **Auto Language Detection**: Automatically detects source language
- **50+ Languages**: Supports translation between 50+ language pairs
- **GPU Acceleration**: Automatically uses CUDA when available for faster translation
- **Robust & Production-Ready**: Zero code duplication, comprehensive error handling

## Quick Start

### Installation

**For users:** Download and install the `.deb` package from [GitHub Releases](https://github.com/costantinoai/evolution-mail-translate/releases)

```bash
# Recommended: lets APT resolve dependencies automatically
sudo apt install ./evolution-translate-extension_1.0.0-1_amd64.deb

# Restart Evolution
killall evolution && evolution &
```

**For developers/advanced users (from source):**

```bash
# Install build dependencies (Ubuntu/Debian)
sudo apt install cmake pkg-config evolution-dev evolution-data-server-dev \
  python3 python3-venv python3-pip

# Clone and build
git clone https://github.com/costantinoai/evolution-mail-translate.git
cd evolution-mail-translate

# Build and install to system directories (requires sudo)
./scripts/install-from-source.sh

# Restart Evolution
killall evolution 2>/dev/null || true
evolution &
```

**Notes:**
- Evolution only loads modules from `/usr/lib*/evolution/modules/`, so installation requires sudo
- The module is installed to `/usr/lib*/evolution/modules/libtranslate-module.so`
- Python helper scripts are installed to `/usr/share/evolution-translate/translate/`
- Python environment and models are per-user: run `evolution-translate-setup` to create a venv under `~/.local/lib/evolution-translate/venv` and install models under `~/.local/share/argos-translate/packages/`

**Uninstall (from source):**

```bash
# From the repository directory
./scripts/uninstall.sh
```

See **[USER_GUIDE.md](USER_GUIDE.md)** for detailed installation instructions and all available methods.

### Usage

1. Select an email and press `Ctrl+Shift+T` (or `Tools` → `Translate Message`)
2. Toggle back to original with `Ctrl+Shift+T` again
3. Configure settings in `Edit` → `Preferences` → `Translate Settings`

See **[USER_GUIDE.md](USER_GUIDE.md)** for complete usage documentation.

## Documentation

- **[USER_GUIDE.md](USER_GUIDE.md)** - Installation, usage, configuration, and troubleshooting
- **[DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)** - Architecture, development, testing, and packaging

## Security & Privacy

- **No Data Transmission**: All translation happens locally; message content never leaves your machine
- **Body Only**: Only email body is processed; headers, addresses, and attachments are never touched
- **Open Source Models**: Uses transparent, auditable open-source translation models
- **No API Keys**: No accounts, no tracking, no telemetry

## Requirements

- **GNOME Evolution** ≥ 3.36
- **Python** 3.8+
- **CMake** 3.10+ (for building from source)

## Contributing

We welcome contributions! See [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) for technical details and development guidelines.

## License

This project follows the same LGPL-2.1+ licensing model as the Evolution example module files.

## Credits

- Built on [ArgosTranslate](https://github.com/argosopentech/argos-translate)
- Integrates with [GNOME Evolution](https://wiki.gnome.org/Apps/Evolution)
- Translation models from [OpenNMT](https://opennmt.net/)
