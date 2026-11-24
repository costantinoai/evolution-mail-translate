# Evolution Translation Extension

Translate email bodies in GNOME Evolution with local Argos models or online providers.

[![Release](https://img.shields.io/github/v/release/costantinoai/evolution-mail-translate)](https://github.com/costantinoai/evolution-mail-translate/releases)
[![License](https://img.shields.io/badge/license-LGPL--2.1%2B-green.svg)](LICENSE)

## Features

- Translate from the mail preview or a separate message window, using the menu, toolbar or keyboard shortcut.
- Keep HTML formatting and switch back to the original message.
- Choose Argos, Google, MyMemory or LibreTranslate in Translate Settings.
- Use installed Argos models if an online request fails, with a visible fallback notice.
- Configure the shortcut once for both window types; changes take effect immediately.
- Build against Evolution 3.52's GtkUIManager or Evolution 3.56+'s EUIManager.

## Install

### Ubuntu 24.04 package

Download the `.deb` from [GitHub Releases](https://github.com/costantinoai/evolution-mail-translate/releases), then run:

```bash
sudo apt install ./evolution-translate-extension_1.2.2-1_amd64.deb
evolution-translate-setup
```

Setup creates a per-user Python environment and asks whether to download default Argos models. For online providers without Argos, use `evolution-translate-setup --online-only` instead. Close and reopen Evolution after installing the module.

### From source

Use a source build for other distributions or Evolution versions:

```bash
sudo apt install build-essential cmake pkg-config libglib2.0-dev libjson-glib-dev libxml2-dev \
  evolution-dev evolution-data-server-dev libebook1.2-dev python3-venv
git clone https://github.com/costantinoai/evolution-mail-translate.git
cd evolution-mail-translate
./scripts/install-from-source.sh
evolution-translate-setup
```

Close and reopen Evolution. The module installs into Evolution's system module directory, so installation requires root; Python setup and models belong to each user. To remove a source installation, run `./scripts/uninstall.sh` from the checkout.

## Use

1. Select a message and press `Ctrl+Shift+T`, click the Translate toolbar button, or choose **Translate → Translate Message**.
2. Press the shortcut again to restore the original, or use **Translate → Show Original**.
3. Open **Translate → Translate Settings** to select the provider, target language, Python environment, model-download preference and shortcut.

The default shortcut remains `Ctrl+Shift+T`. Click its field in Translate Settings to change it; Backspace clears it. Settings apply to the preview and separate message windows.

## Privacy and providers

**Argos processes message text locally. Google, MyMemory and LibreTranslate send the body text to the selected service.** New installations default to Google; select Argos for local translation. Headers and attachment contents are excluded from translation.

Argos requires a model for the language pair. Downloading models needs a network connection; translating with installed models does not. Online services can impose quotas or require credentials. LibreTranslate accepts a custom instance through `LIBRE_TRANSLATE_URL` and an optional `LIBRE_API_KEY`, inherited from Evolution's environment.

If an online provider fails, the extension tries installed Argos models without downloading a new model. It reports a successful fallback or shows the provider error. It does not silently switch to another online service.

## Requirements and documentation

- Evolution 3.52 or later, with matching development headers for a source build.
- CMake 3.15 or later, a C compiler and the development libraries listed above.
- Python 3 with `venv`; setup installs the dependencies for the selected backends.

See the [User Guide](docs/USER_GUIDE.md) for settings and troubleshooting, the [Developer Guide](docs/DEVELOPER_GUIDE.md) for tests and releases, and the [changelog](docs/CHANGELOG.md) for changes. Contributions follow [CONTRIBUTING.md](docs/CONTRIBUTING.md).

## License and credits

The project is licensed under LGPL-2.1-or-later; [LICENSE](LICENSE) contains the complete LGPL 2.1 text. Original copyright notices remain in the source. Adapted UI, shortcut, compute-type and rendering changes from [MoDD0's fork](https://github.com/MoDD0/moddo-evolution-translator) retain commit references and dated modification notices in the affected files.

Built on [GNOME Evolution](https://gitlab.gnome.org/GNOME/evolution) and [Argos Translate](https://github.com/argosopentech/argos-translate), with online providers accessed through [deep-translator](https://github.com/nidhaloff/deep-translator) or the LibreTranslate API.
