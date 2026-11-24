# Changelog

## 1.2.2

- Repair action registration for Evolution 3.56+ while retaining the legacy UI.
- Add Translate toolbar buttons and one configurable shortcut for both window types.
- Validate optional Argos compute types before import, including CPU `int8`.
- Keep translated messages readable under dark desktops while preserving message colors and raw cache content.
- Restore the complete LGPL 2.1 text and retain fork provenance and dated port notices.
- Correct provider privacy, setup, testing and release documentation; add opt-in live provider checks.
- Replace the redirected LibreTranslate default URL with the official API and explain non-JSON instance responses.
- Check both Evolution UI APIs in CI.

## 1.2.1

- Restore the Python environment and provider protocol, actionable errors and installed-model Argos fallback.
- Guard asynchronous results against message changes and window closure; correct MIME body selection and cache identity.
- Share provider transport and HTML traversal; enforce build, installation and package checks.

## 1.0.0
- Initial public release of Evolution Translation Extension.
- ArgosTranslate offline translation with HTML preservation.
- Per-user Python environment via `evolution-translate-setup`.
- Debian packaging and build scripts.
