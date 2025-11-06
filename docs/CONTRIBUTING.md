# Contributing

- Follow GNOME C coding style; tabs width 8; max line length ~120.
- Use SPDX headers on new files: `SPDX-License-Identifier: LGPL-2.1-or-later`.
- Keep changes minimal and focused; prefer DRY and clear ownership of modules.
- Add or update tests for changes that affect behavior.
- Ensure CI is green before requesting review.

## Development

- Build: `cmake -S . -B build && cmake --build build -j`
- Install from source: `./scripts/install-from-source.sh`
- User setup (Python deps/models): `evolution-translate-setup`
- Run Python tests: `pytest -q`

## Reporting Issues

Please include:
- Evolution version and distro
- Steps to reproduce
- Logs with `G_MESSAGES_DEBUG=all`
- If translation-related: input sample and target language

