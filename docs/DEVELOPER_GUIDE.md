# Developer Guide

## Build and install

The source requires Evolution 3.52+ and matching development packages. CMake chooses GtkUIManager below 3.56 and EUIManager from 3.56 onward.

```bash
sudo apt install build-essential cmake pkg-config libglib2.0-dev libjson-glib-dev libxml2-dev \
  evolution-dev evolution-data-server-dev libebook1.2-dev python3-venv
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Werror"
cmake --build build -j2
```

Use `./scripts/install-from-source.sh` to install, then run `evolution-translate-setup` as the ordinary user. Close and reopen Evolution to load the module. `./scripts/uninstall.sh` removes files recorded by the source installation.

## Tests

Tests are tracked in `tests/`. The GUI tests launch the real Evolution executable on private Xvfb displays with isolated XDG directories, a memory settings backend and synthetic local messages. They do not use the current desktop's mail session.

```bash
sudo apt install xvfb libxtst6 dbus-x11
python3 -m venv .venv
.venv/bin/python -m pip install pytest ruff==0.15.1 -r requirements-online.txt
.venv/bin/ruff check tools/translate tests
for script in scripts/*.sh; do bash -n "$script"; done
.venv/bin/python -m pytest -q -p no:cacheprovider tests
```

The GUI harness needs an owned, mode-0700 `/run/user/$(id -u)` directory, normally supplied by a login session. In CI or a container, create it explicitly. XTEST supplies real keyboard events only inside each test's private display.

For another build directory, set both `TRANSLATE_TEST_MODULE` to its `src/libtranslate-module.so` and `TRANSLATE_TEST_BUILD` to the build directory. `TRANSLATE_GUI_EVIDENCE_DIR` saves the GUI logs and screenshots. A private test can use installed Argos models with `TRANSLATE_GUI_USE_MODELS=1`; this requires the default per-user Argos environment and a Dutch-to-English model.

### Optional online smoke checks

Normal tests simulate provider responses and skip the two live checks. To send a fixed synthetic French sentence to MyMemory and LibreTranslate:

```bash
TRANSLATE_LIVE_PROVIDERS=1 .venv/bin/python -m pytest -q tests/test_live_providers.py
```

Use `-k mymemory` or `-k libre` to check one service. LibreTranslate uses `LIBRE_TRANSLATE_URL` and an optional `LIBRE_API_KEY` from the process environment; the tests never add credentials to command arguments. Service quotas, authentication and outages can fail these checks independently of the local regression suite. Do not use real email text for smoke checks.

The helper uses the test interpreter by default. Set `TRANSLATE_PYTHON_BIN` to test an existing translation environment without installing pytest into it.

## Continuous integration and packaging

CI runs the same job on Ubuntu 24.04 and 26.04 to cover both Evolution UI APIs. Shell syntax, pinned Ruff, warning-as-error compilation, tests, Debian packaging and Lintian errors are blocking. Live provider checks remain opt-in.

```bash
sudo apt install debhelper devscripts lintian chrpath
./scripts/build-deb.sh
```

The build script snapshots tracked and non-ignored source files into a unique directory under `build/debian/`, builds there and copies artifacts into `packages/`. Private task ledgers are excluded. The distributed binary targets Ubuntu 24.04; build from source against other Evolution versions.

## Release

`debian/changelog` is the version source. Commit the tested source changes first, then preview the release:

```bash
./scripts/prepare-release.sh 1.2.2 --dry-run
./scripts/prepare-release.sh 1.2.2 --no-push
```

Preparation requires a clean tree. It creates the `1.2.2-1` changelog entry, a release commit and the exact annotated tag `v1.2.2`. It does not publish by default. Verify the final version and checks before pushing:

```bash
git push origin HEAD
git push origin refs/tags/v1.2.2
```

Alternatively, `--push` publishes immediately after preparation; use it only when publication is authorized. The tag workflow runs the complete CI matrix, builds the Ubuntu 24.04 package and source archive, checks package quality and publishes both artifacts with SHA256 files. Confirm the remote tag, workflow results, downloadable assets and checksums before treating the release as complete.

## Licensing and contributions

Use `SPDX-License-Identifier: LGPL-2.1-or-later` on new source files. Preserve existing copyright notices, record the source of adapted code and add dated modification notices. The 2026-11-24 license correction restores the complete LGPL 2.1 document, including section 5, from Debian's `/usr/share/common-licenses/LGPL-2.1`; it does not change the project's LGPL-2.1-or-later declaration.

Follow [CONTRIBUTING.md](CONTRIBUTING.md). Bug reports should include the distribution, Evolution version, selected provider, steps to reproduce and sanitized errors. Use synthetic examples rather than private messages or credentials.
