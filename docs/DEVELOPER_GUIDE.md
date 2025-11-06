# Developer Guide

This guide outlines development setup, build/package steps, contribution guidelines, and how to report issues.

## Development Setup

- Dependencies (Ubuntu/Debian):
  - `cmake`, `pkg-config`
  - `glib2.0-dev`, `libjson-glib-dev`
  - `evolution-dev`, `evolution-data-server-dev`
  - `python3`, `python3-venv`, `python3-pip` (for helper tools)

```bash
sudo apt install cmake pkg-config glib2.0-dev libjson-glib-dev \
  evolution-dev evolution-data-server-dev python3 python3-venv python3-pip
```

## Build From Source

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j
sudo cmake --install .

# Restart Evolution
killall evolution 2>/dev/null || true
evolution &
```

Helpers are installed to `/usr/share/evolution-translate/translate/`. Per‑user Python env and models are prepared with `evolution-translate-setup`.

## Packaging (Debian)

```bash
dpkg-buildpackage -us -uc -b
ls -l ../*.deb
```

## Contributing

- Follow GNOME C coding style; tabs width 8; max line length ~120.
- Use SPDX headers on new files: `SPDX-License-Identifier: LGPL-2.1-or-later`.
- Keep changes minimal and focused; prefer DRY and clear module ownership.
- Add/update docs when changing user-observable behavior.

### Development

- Build: `cmake -S . -B build && cmake --build build -j`
- Install from source: `./scripts/install-from-source.sh`
- User setup (Python deps/models): `evolution-translate-setup`

### Reporting Issues

Please include:
- Evolution version and distro
- Steps to reproduce
- Logs with `G_MESSAGES_DEBUG=all`
- If translation-related: input sample and target language

