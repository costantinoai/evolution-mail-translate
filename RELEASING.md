# Releasing

This project publishes a `.deb` on GitHub Releases when you push a semver tag.

## Versioning and Tagging

1. Update versions:
   - `CMakeLists.txt` → `set(VERSION "X.Y.Z")`
   - `debian/changelog` → `dch -v X.Y.Z-1 "Release X.Y.Z"`
   - Update `CHANGELOG.md`
2. Commit and tag:
   ```bash
   git commit -am "release: vX.Y.Z"
   git tag -a vX.Y.Z -m "Release vX.Y.Z"
   git push && git push --tags
   ```

The `release` GitHub Action builds the Debian package and uploads the `.deb` and a `.sha256` to the GitHub Release for tag `vX.Y.Z`.

## Building Locally

```bash
sudo apt-get install -y cmake pkg-config glib2.0-dev libjson-glib-dev \
  evolution-dev evolution-data-server-dev debhelper devscripts
dpkg-buildpackage -us -uc -b
ls -l ../*.deb
```

## Optional (Source Tarball)

You can build a tarball with:
```bash
cmake -S . -B build && cmake --build build --target dist
```

## Notes

- The GitHub build uses `-us -uc` (no signing). If you need signed artifacts, configure GPG in CI and adjust flags.
- For multiple architectures, build on separate runners (e.g., amd64 and arm64) and upload both `.deb` files to the same release.

