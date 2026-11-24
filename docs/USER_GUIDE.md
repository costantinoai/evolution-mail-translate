# User Guide

## Installation

On Ubuntu 24.04, download the package from [Releases](https://github.com/costantinoai/evolution-mail-translate/releases):

```bash
sudo apt install ./evolution-translate-extension_1.2.2-1_amd64.deb
evolution-translate-setup
```

For a source build, follow the [README](../README.md#from-source). Close and reopen Evolution after installation. Each user runs setup separately; setup does not need root.

Setup installs Python dependencies into `~/.local/lib/evolution-translate/venv` and asks whether to install default Argos models. Use `--models` or `--no-models` to choose without a prompt. Use `--online-only` to install online dependencies without Argos; it cannot be combined with `--models`.

## Translation and settings

- Select an email, then use `Ctrl+Shift+T`, the Translate toolbar button, or **Translate → Translate Message**.
- Use the same shortcut again or **Translate → Show Original** to restore the message. The preview also provides `Ctrl+Shift+O` for Show Original.
- Open **Translate → Translate Settings** to choose the target language and provider, allow model downloads, set a Python environment directory or change the shortcut.

Click the shortcut field and press the desired combination. Backspace disables it. Save applies the binding to open preview and message windows; Cancel leaves settings unchanged. The default is `Ctrl+Shift+T`.

The Python environment field takes the directory containing `bin/python`, not the interpreter filename. Leave it empty to use the default environment. Changing messages or closing the originating window cancels its pending translation.

## Choosing a provider

| Provider | Where message text is processed | Requirements |
|---|---|---|
| Argos | On your computer | Argos dependencies and a model for the language pair |
| Google | Online | Online dependencies and access to the service |
| MyMemory | Online | Online dependencies, a supported language pair and available quota |
| LibreTranslate | At the selected instance | Online dependencies; instance access and possibly an API key |

New installations default to Google. Select Argos in Translate Settings for local translation. Only body content is submitted for translation; headers and attachments are excluded. Model downloads require a network connection even when Argos is selected.

An online failure triggers a fallback attempt using already-installed Argos models. It does not download a model or contact another online provider. A successful fallback is announced in the originating window; otherwise an error explains the failure.

## Troubleshooting

- **Python environment not found or missing dependencies:** run `evolution-translate-setup`, or correct the environment directory in Translate Settings. Use `--online-only` if Argos is unnecessary.
- **Missing Argos model:** run `evolution-translate-setup --models` for default models, or enable on-demand downloads for direct Argos requests. Available language pairs depend on installed models.
- **HTTP 429 / rate limit:** wait before retrying or select Argos with an installed model. The extension reports quota failures instead of repeatedly submitting the same request.
- **LibreTranslate instance or key errors:** set `LIBRE_TRANSLATE_URL` to the instance base URL and, when required, `LIBRE_API_KEY` in the environment used to launch Evolution. Do not put credentials in bug reports.
- **Unchanged display after upgrading:** close and reopen Evolution so it loads the installed module.

Translated messages have readable light defaults under dark desktops. A message's own CSS and declared color scheme take precedence. Show Original restores Evolution's original message display.

## Locations and advanced options

| Item | Default location |
|---|---|
| Module | `/usr/lib*/evolution/modules/libtranslate-module.so` |
| Helpers | `/usr/share/evolution-translate/translate/` |
| Python environment | `~/.local/lib/evolution-translate/venv` |
| Argos models | `~/.local/share/argos-translate/packages/` |

For development, `TRANSLATE_HELPER_PATH` selects a helper and `TRANSLATE_PYTHON_BIN` selects its interpreter. `ARGOS_COMPUTE_TYPE=int8` requests quantized inference; accepted compute types still depend on the hardware and installed engine. Invalid names are ignored with a warning before Argos imports its settings. Leave the variable unset to keep the engine default.
