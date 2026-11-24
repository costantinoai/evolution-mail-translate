#!/bin/bash
# Evolution Translation Extension - Developer Installation Script
# Builds and installs into system directories (/usr) where Evolution can find modules

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# System locations (Evolution only loads modules from /usr/lib*/evolution/modules/)
INSTALL_PREFIX="/usr"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

echo "Checking dependencies..."

MISSING=()
command -v cmake >/dev/null 2>&1 || MISSING+=("cmake")
command -v pkg-config >/dev/null 2>&1 || MISSING+=("pkg-config")
pkg-config --exists evolution-shell-3.0 2>/dev/null || MISSING+=("evolution-dev" "evolution-data-server-dev")
command -v python3 >/dev/null 2>&1 || MISSING+=("python3" "python3-venv" "python3-pip")

if [ ${#MISSING[@]} -ne 0 ]; then
  print_error "Missing dependencies: ${MISSING[*]}"
  echo "  Install with: sudo apt install ${MISSING[*]}"
  exit 1
fi
print_status "Build prerequisites found"

echo ""
echo "Building extension (Debug mode for development)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
      ..

cmake --build "$BUILD_DIR" --parallel 2
print_status "Build completed"

echo ""
echo "Installing extension to /usr (requires sudo)..."
sudo cmake --install "$BUILD_DIR"

print_status "Extension installed to /usr/lib*/evolution/modules/"

echo ""
echo "Compiling GSettings schemas (requires sudo)..."
# IMPORTANT: Use umask 0022 to ensure schemas are world-readable (644)
# Without this, restrictive root umask may create 640 files that users can't read
sudo sh -c 'umask 0022 && glib-compile-schemas /usr/share/glib-2.0/schemas'
print_status "GSettings schemas compiled"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
print_status "Installation completed successfully!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Next steps:"
echo "  1. Restart Evolution:"
echo "     killall evolution 2>/dev/null; evolution &"
echo ""
echo "  2. The extension will load automatically"
echo ""
echo "  3. To use the extension:"
echo "     - Select an email message"
echo "     - Press Ctrl+Shift+T or go to Translate → Translate Message"
echo ""
echo "  4. Configure settings:"
echo "     - Go to Translate → Translate Settings"
echo ""
echo "Set up Python dependencies and models (per user):"
echo "  evolution-translate-setup"
echo ""
echo "Environment variables for testing (optional):"
echo "  export TRANSLATE_HELPER_PATH=\"$PROJECT_ROOT/tools/translate/translate_runner.py\""
echo "  export TRANSLATE_PYTHON_BIN=\"$HOME/.local/lib/evolution-translate/venv/bin/python\""
echo ""
echo "To uninstall, run: $SCRIPT_DIR/uninstall.sh"
