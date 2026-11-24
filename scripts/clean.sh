#!/usr/bin/env bash
# Preview scoped build/cache cleanup; deletion requires --apply.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APPLY=false
case "${1:---dry-run}" in
  --apply) APPLY=true ;;
  --dry-run) ;;
  *) echo "Usage: clean.sh [--dry-run|--apply]" >&2; exit 2 ;;
esac
if (( $# > 1 )); then echo "Only one option is allowed." >&2; exit 2; fi
for name in build _build tools/translate/__pycache__ tests/__pycache__; do
  target="$ROOT_DIR/$name"
  if [[ -d "$target" && ! -L "$target" ]]; then
    printf '%s: %s\n' "$(if "$APPLY"; then echo Removing; else echo Would-remove; fi)" "$target"
    if "$APPLY"; then rm -rf -- "$target"; fi
  fi
done
