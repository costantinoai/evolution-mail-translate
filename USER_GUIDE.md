# Evolution Translation Extension - User Guide

Complete guide for installing, configuring, and using the Evolution Translation Extension.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
  - [Method 1: Install from .deb Package (Recommended)](#method-1-install-from-deb-package-recommended)
  - [Method 2: Install from Source](#method-2-install-from-source)
  - [Installation Comparison](#installation-comparison)
- [Usage](#usage)
- [Configuration](#configuration)
- [Language Model Management](#language-model-management)
- [Troubleshooting](#troubleshooting)
- [FAQs](#faqs)

---

## Overview

The Evolution Translation Extension adds instant email translation capabilities to GNOME Evolution, allowing you to translate foreign language emails into your preferred language with a single click. All translation happens locally on your machine—no internet required for translation, ensuring complete privacy.

## Features

- **One-Click Translation**: Translate emails directly within Evolution
- **100% Offline & Private**: Uses local translation models (ArgosTranslate)—no data leaves your computer
- **Install-on-Demand**: Automatically downloads missing translation models as needed
- **HTML Email Support**: Preserves formatting, styles, and structure in translated emails
- **Auto Language Detection**: Automatically detects source language
- **50+ Languages**: Supports translation between 50+ language pairs
- **GPU Acceleration**: Automatically uses CUDA when available for faster translation
- **Toggle View**: Easily switch between translated and original content

---

## Installation

Choose the installation method that best suits your needs:

### Method 1: Install from .deb Package (Recommended)

**Best for:** End users who just want to use the extension.

#### Download and Install

```bash
# Download from GitHub Releases
wget https://github.com/costantinoai/evolution-mail-translate/releases/download/v1.0.0/evolution-translate-extension_1.0.0-1_amd64.deb

# Install the package (lets APT resolve dependencies)
sudo apt install ./evolution-translate-extension_1.0.0-1_amd64.deb

# Restart Evolution
killall evolution
evolution &
```

#### What Gets Installed Automatically

✅ Evolution extension module
✅ Python virtual environment with ArgosTranslate
✅ Default translation models (English ↔ Spanish, French, German)
✅ All dependencies

**Installation location:** `/usr/lib/evolution-translate/`

#### Uninstall

```bash
# Remove extension
sudo apt remove evolution-translate-extension

# Or remove with all configuration
sudo apt purge evolution-translate-extension
```

---

### Method 2: Install from Source

**Best for:** Developers who want to modify the code or contribute.

#### Quick Install (Automated)

```bash
# Clone repository
git clone https://github.com/costantinoai/evolution-mail-translate.git
cd evolution-mail-translate

# Run installation script (requires sudo)
./scripts/install-from-source.sh

# Restart Evolution
killall evolution 2>/dev/null; evolution &
```

The script automatically:
- Checks for dependencies
- Builds the extension in Debug mode
- Installs to `/usr/lib*/evolution/modules/` (requires sudo)
- Sets up system Python environment
- Downloads translation models

**Note:** Evolution only loads modules from `/usr/lib*/evolution/modules/`, so sudo is required.

#### Manual Install (Step by Step)

```bash
# Install build dependencies
sudo apt install evolution-dev evolution-data-server-dev cmake pkg-config python3 python3-venv

# Build
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)
sudo make install

# Set up Python venv (as root)
sudo python3 -m venv /usr/lib/evolution-translate/venv
sudo /usr/lib/evolution-translate/venv/bin/pip install argostranslate translatehtml langdetect
sudo chmod -R 755 /usr/lib/evolution-translate/venv

# Install models (as your user)
/usr/lib/evolution-translate/venv/bin/python \
  /usr/lib/evolution-translate/translate/install_default_models.py

# Compile schemas
sudo glib-compile-schemas /usr/share/glib-2.0/schemas

# Restart Evolution
killall evolution 2>/dev/null; evolution &
```

**Installation location:** `/usr/lib*/evolution/modules/libtranslate-module.so`

#### Uninstall

```bash
./scripts/uninstall.sh
```

---

### Installation Comparison

| Feature | .deb Package | From Source |
|---------|--------------|-------------|
| **Target users** | End users | Developers |
| **Dependencies** | Auto-installed | Manual setup |
| **Install location** | `/usr/` (system-wide) | `/usr/` (system-wide) |
| **Python setup** | Automatic venv | Automatic venv |
| **Models** | Auto-downloaded | Auto-downloaded |
| **Updates** | Via package manager | `git pull && rebuild` |
| **Uninstall** | `apt remove` | `./scripts/uninstall.sh` |
| **Permissions** | Requires `sudo` | Requires `sudo` |
| **Build type** | Release | Debug (for development) |

---

## Usage

### Translating an Email

1. **Open Evolution** and select a mail folder
2. **Select an email** you want to translate
3. **Click the Translate button** (or use the menu):
   - **Toolbar**: Click the "Translate Message" button
   - **Menu**: Select `Translate` → `Translate Message`
   - **Keyboard**: Press `Ctrl+Shift+T`

The email will be translated and displayed in-place, maintaining all original formatting.

### Viewing the Original

After translating an email, you can easily switch back to the original:

1. Click **"Show Original"** in the Translate menu or press `Ctrl+Shift+T` again
2. You can toggle between translated and original versions as many times as needed

---

## Configuration

### Configuring Translation Settings

1. **Open Settings**:
   - Go to `Edit` → `Preferences` → `Translate Settings`
   - Or `Tools` → `Translate Settings…`

2. **Set Target Language**:
   - Choose your preferred target language from the dropdown
   - Default is English (en)
   - Changes take effect immediately

3. **Install-on-Demand Setting**:
   - Enable to automatically download missing translation models
   - Disable if you want manual control over model downloads

### Supported Languages

The extension supports translation between 50+ languages including:
- **European**: English, Spanish, French, German, Italian, Portuguese, Russian
- **Asian**: Chinese, Japanese, Korean, Hindi, Arabic, Thai, Vietnamese
- **And many more**: Dutch, Polish, Turkish, Swedish, Norwegian, etc.

---

## Language Model Management

### Installing Additional Language Pairs

To translate between specific language pairs, you need to install the corresponding models:

```bash
source .venv/bin/activate
python tools/translate/setup_models.py
```

The setup script will prompt you to:
1. Select source language
2. Select target language
3. Download and install the model

### Model Storage

Models are stored in:
```
~/.local/share/argos-translate/packages/
```

### Removing Models

To free up space, you can remove unused models:
```bash
rm -rf ~/.local/share/argos-translate/packages/<language-pair>
```

## Troubleshooting

### Translation Not Working

1. **Check Python environment**:
   ```bash
   which python
   # Should point to your venv
   ```

2. **Verify models are installed**:
   ```bash
   python -c "import argostranslate.package; print(argostranslate.package.get_installed_packages())"
   ```

3. **Check Evolution logs**:
   ```bash
   evolution 2>&1 | grep translate
   ```

### Translation is Slow

- **First translation**: The first translation after starting Evolution may be slow as models load into memory
- **Subsequent translations**: Should be much faster (typically 1-3 seconds)
- **GPU Acceleration**: If you have a CUDA-capable GPU, the extension will automatically use it for faster translation

### HTML Formatting Issues

If translated HTML emails appear broken:

1. Try translating again (sometimes helps with complex HTML)
2. Use "Show Original" to see the original formatting
3. Check that BeautifulSoup4 is installed: `pip list | grep beautifulsoup4`

### Email Doesn't Translate

Common reasons:
- Email is already in target language (extension skips translation)
- Email contains no translatable text (images only, etc.)
- Language model not installed for detected source language

**Solution**: Check Evolution's output for error messages:
```bash
evolution 2>&1 | grep -i translate
```

### Evolution Crashes on Startup (Source Install)

**Symptom**: Evolution crashes immediately with "Settings schema 'org.gnome.evolution.shell' is not installed"

**Cause**: GSettings schema file has incorrect permissions (only readable by root)

**Solution**:
```bash
# Check schema permissions
ls -l /usr/share/glib-2.0/schemas/gschemas.compiled
# Should be: -rw-r--r-- (world-readable)

# If showing -rw-r----- (not world-readable), fix it:
sudo sh -c 'umask 0022 && glib-compile-schemas /usr/share/glib-2.0/schemas'

# Verify schemas are now readable
gsettings list-schemas | grep evolution | wc -l
# Should show 40+ schemas

# Restart Evolution
killall evolution
evolution &
```

**Prevention**: The install script now uses `umask 0022` to prevent this issue.

---

## Tips and Best Practices

1. **Install Common Language Pairs**: Pre-install models for languages you frequently encounter
2. **Use Keyboard Shortcut**: `Ctrl+Shift+T` is fastest for translation
3. **Check Original**: For important emails, always verify by checking the original
4. **HTML Formatting**: Translation preserves all HTML formatting, including links, images, and styles
5. **Privacy**: All translation happens locally—no data sent to external servers
6. **First Translation**: The first translation may take longer as models load into memory
7. **GPU Acceleration**: If you have a CUDA-capable GPU, it will be used automatically for faster translation

## Keyboard Shortcuts

- **`Ctrl+Shift+T`** - Translate current message / Toggle back to original
- **`Alt+T`** - Open Translate menu (if configured)

---

## FAQs

### Q: Does this send my emails to the cloud?
**A:** No, all translation happens locally using open-source models. No data ever leaves your machine.

### Q: How much disk space do models use?
**A:** Each language pair model is approximately 100-300 MB.

### Q: Can I translate sent emails?
**A:** Yes, the translation works on any email you can view in Evolution, including sent emails.

### Q: What's the translation quality like?
**A:** Quality varies by language pair. Common pairs (e.g., French↔English) are quite good. Less common pairs may be less accurate.

### Q: Can I use online translation services instead?
**A:** Currently, only offline ArgosTranslate is supported. Online translation providers may be added in future versions.

### Q: How do I uninstall the extension?
**A:**
- If installed from package: `sudo apt remove evolution-translate-extension`
- If installed from source: `./scripts/uninstall.sh`

### Q: Will this work with Evolution Flatpak?
**A:** Currently, the extension is designed for system-installed Evolution. Flatpak support may be added in the future.

---

## Getting Help

- **Report Issues**: [GitHub Issues](https://github.com/costantinoai/evolution-mail-translate/issues)
- **Developer Documentation**: See [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) for technical details and contribution guidelines

---

## Credits

- Built on [ArgosTranslate](https://github.com/argosopentech/argos-translate)
- Integrates with [GNOME Evolution](https://wiki.gnome.org/Apps/Evolution)
- Translation models from [OpenNMT](https://opennmt.net/)
