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

## Testing

### Local Testing

- Standard tests: `tests/run-tests.sh --standard`
- Docker tests (local, test-only): see `tests/docker/`
  - Self-contained: `tests/docker/run-all-images.sh` (Ubuntu 24.04)
  - Base images: `tests/docker/build-and-test.sh` (Ubuntu 24.04)

**Note:** The `tests/` folder is gitignored and not tracked in the repository; it exists purely for local validation and experimentation.

### Continuous Integration

The CI workflow runs on every push and pull request with a two-stage approach:

1. **Quick Check Stage** (fails fast, ~2-3 minutes):
   - Python linting (black, flake8)
   - Bash syntax validation
   - Basic compilation check with warnings as errors

2. **Full Build Stage** (only runs if quick-check passes, ~5-7 minutes):
   - Complete build with all compiler warnings enabled
   - C static analysis (cppcheck)
   - Python unit tests
   - Debian package generation
   - Lintian package validation

**All checks are blocking** - the CI will fail if any validation step fails. This ensures code quality and prevents broken builds from being merged.

## Building Packages for Release

It’s acceptable to build `.deb` packages inside Docker as long as you use a clean base image matching the target distro (e.g., Ubuntu 24.04) and a reproducible process. Many projects follow this pattern. For Debian-style workflows, you can also use `sbuild`/`pbuilder` or a CI-based build.

Signing is separate from building: produce unsigned packages in containers, then sign artifacts on your release machine or in CI.

## Packaging (Debian)

```bash
dpkg-buildpackage -us -uc -b
ls -l ../*.deb
```

## Release Process

Releases are **fully automated** using GitHub Actions. When you push a tag matching `vX.Y` (e.g., `v1.1`, `v1.2`), the release workflow automatically builds packages and publishes them. Patch tags like `v1.1.1` are ignored.

### Automated Release Script (Recommended)

Use the automated release preparation script to handle the entire process:

```bash
./scripts/prepare-release.sh 1.2.0
```

This script will:
1. Validate the version format
2. Update `CMakeLists.txt` version
3. Update `debian/changelog` with proper formatting
4. Validate version consistency across files
5. Commit changes with conventional commit message
6. Create annotated git tag (`v1.2`)
7. Push to remote, triggering the automated release workflow

**Options:**
- `--dry-run`: Preview changes without applying them
- `--no-push`: Create tag locally but don't push (manual control)
- `--help`: Show detailed usage information

**Example:**
```bash
# Preview what will happen
./scripts/prepare-release.sh 1.2.0 --dry-run

# Create release (after testing locally)
./scripts/prepare-release.sh 1.2.0
```

### Manual Release Process

If you prefer manual control, follow these steps:

1. **Update version files:**
   ```bash
   # Update CMakeLists.txt
   sed -i 's/set(VERSION ".*")/set(VERSION "1.2.0")/' CMakeLists.txt

   # Update debian/changelog
   dch -v 1.2.0-1 "Release version 1.2.0"
   ```

2. **Commit changes:**
   ```bash
   git add CMakeLists.txt debian/changelog
   git commit -m "chore: release version 1.2.0"
   ```

3. **Create and push tag:**
   ```bash
   git tag -a v1.2 -m "Release 1.2.0"
   git push origin main v1.2
   ```

### What Happens Automatically

Once you push the tag, GitHub Actions will:

1. **Build on clean Ubuntu 24.04 environment**
   - Ensures reproducible builds
   - All dependencies installed fresh

2. **Generate release artifacts:**
   - Debian package (`.deb`) for amd64
   - Source tarball (`.tar.xz`) via CMake dist target
   - SHA256 checksums for both artifacts

3. **Validate package quality:**
   - Lintian checks (Debian policy compliance)
   - Errors are fatal (build fails if package is malformed)

4. **Create GitHub Release:**
   - Release name: `evolution-translate-extension X.Y.Z-1 (amd64)`
   - Includes installation instructions
   - Attaches all artifacts
   - Links to documentation

5. **Monitor the release:**
   - View workflow progress: https://github.com/costantinoai/evolution-mail-translate/actions
   - Access release when complete: https://github.com/costantinoai/evolution-mail-translate/releases

### Supported Platforms

- **Official release target:** Ubuntu 24.04 LTS (Evolution 3.52+)
- **From source:** Any Linux distribution with Evolution 3.52+ and build dependencies

**Note:** Ubuntu 22.04 has older Evolution headers and is not officially supported. Users on 22.04 should build from source or upgrade to 24.04.

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
