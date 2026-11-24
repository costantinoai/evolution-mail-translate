#!/usr/bin/env bash
# Prepare a release locally; publishing requires the explicit --push option.
set -euo pipefail
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION_ARG=""
DRY_RUN=false
PUSH=false
for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=true ;;
    --push) PUSH=true ;;
    --no-push) PUSH=false ;;
    --help|-h)
      echo "Usage: prepare-release.sh MAJOR.MINOR.PATCH [--dry-run] [--push|--no-push]"
      echo "The default prepares a local commit and exact version tag without publishing."
      exit 0 ;;
    -*) echo "Unknown option: $arg" >&2; exit 2 ;;
    *) if [[ -n "$VERSION_ARG" ]]; then echo "Only one version is allowed." >&2; exit 2; fi
       VERSION_ARG="$arg" ;;
  esac
done
if [[ ! "$VERSION_ARG" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "A MAJOR.MINOR.PATCH version is required." >&2
  exit 2
fi
cd "$PROJECT_ROOT"
command -v git >/dev/null
TAG="v$VERSION_ARG"
if git show-ref --verify --quiet "refs/tags/$TAG"; then
  echo "Tag $TAG already exists." >&2
  exit 1
fi
printf 'Version source: debian/changelog\nRelease version: %s-1\nTag: %s\n' "$VERSION_ARG" "$TAG"
if "$DRY_RUN"; then
  echo "Would update debian/changelog, commit that file, and create the tag above."
  if "$PUSH"; then echo "Would push the current branch and that exact tag."; else echo "No push requested."; fi
  exit 0
fi
for tool in dch dpkg-parsechangelog; do
  command -v "$tool" >/dev/null || { echo "Missing release tool: $tool" >&2; exit 1; }
done
if [[ -n "$(git status --porcelain)" ]]; then
  echo "Commit or resolve existing changes before preparing a release." >&2
  exit 1
fi
dch --newversion "$VERSION_ARG-1" --distribution unstable "Release version $VERSION_ARG"
[[ "$(dpkg-parsechangelog -S Version)" == "$VERSION_ARG-1" ]]
git add -- debian/changelog
git commit -m "chore: release $VERSION_ARG"
git tag -a "$TAG" -m "Evolution Translation Extension $VERSION_ARG"
if "$PUSH"; then
  git push origin HEAD
  git push origin "refs/tags/$TAG"
else
  printf 'Prepared %s locally. Nothing was pushed.\n' "$TAG"
fi
