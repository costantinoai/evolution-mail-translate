# Changelog

All notable changes to this project will be documented in this file.

## [1.0.0] - 2025-11-06

- Security: Remove PATH fallback for helper; fail closed if helper/python not found.
- Fix: Subprocess reference leak in Argos provider.
- Robustness: Add parameter validation and safer finish handling.
- Logging: Reduce path disclosure to debug-level logs.
- Packaging: Move Python helper scripts to `/usr/share/evolution-translate/translate/`.
- Packaging: Remove pip/venv and model downloads from Debian maintainer scripts.
- Tooling: Add `evolution-translate-setup` for per-user environment and models.
- CI: Add GitHub Actions workflow for build, tests, and package linting.
- Docs: Update README/USER/DEVELOPER guides to reflect new setup.
- Licensing: Add repository root `LICENSE` (LGPL-2.1).

