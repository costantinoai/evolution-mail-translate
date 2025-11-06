#!/bin/bash
# Evolution Translation Extension - Developer Installation Script
# Builds and installs into system directories (/usr) where Evolution can find modules

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# System locations (Evolution only loads modules from /usr/lib*/evolution/modules/)
INSTALL_PREFIX="/usr"
SYSTEM_TRANSLATE_ROOT="/usr/lib/evolution-translate"
VENV_DIR="$SYSTEM_TRANSLATE_ROOT/venv"

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

make -j$(nproc)
print_status "Build completed"

echo ""
echo "Installing extension to /usr (requires sudo)..."
sudo make install
print_status "Extension installed to /usr/lib*/evolution/modules/"

echo ""
echo "Setting up system Python virtual environment (requires sudo)..."
sudo mkdir -p "$SYSTEM_TRANSLATE_ROOT"

# Create venv if missing or invalid
if [ ! -x "$VENV_DIR/bin/python" ] || [ ! -f "$VENV_DIR/pyvenv.cfg" ]; then
    if ! sudo /usr/bin/python3 -m venv "$VENV_DIR"; then
        print_error "Failed to create venv at $VENV_DIR"
        exit 1
    fi
    print_status "Virtual environment created at $VENV_DIR"
else
    print_warning "Virtual environment already exists at $VENV_DIR"
fi

# Bootstrap pip and install dependencies
echo "Bootstrapping pip in virtual environment..."
sudo "$VENV_DIR/bin/python" -m ensurepip --upgrade >/dev/null 2>&1 || true
echo "Upgrading pip..."
sudo "$VENV_DIR/bin/python" -m pip install --upgrade pip --quiet

echo ""
echo "Installing Python dependencies (this may take a few minutes)..."
echo "  - argostranslate (will download package indices)"
echo "  - translatehtml"
echo "  - langdetect"
echo ""

if ! sudo "$VENV_DIR/bin/python" -m pip install argostranslate translatehtml langdetect; then
    if "$VENV_DIR/bin/python" -m pip --help 2>/dev/null | grep -q "break-system-packages"; then
        print_warning "Retrying dependency install with --break-system-packages"
        sudo "$VENV_DIR/bin/python" -m pip install --break-system-packages argostranslate translatehtml langdetect
    else
        print_error "Failed to install Python dependencies"
        exit 1
    fi
fi

echo ""
sudo chmod -R 755 "$VENV_DIR"
print_status "Python dependencies installed into $VENV_DIR"

echo ""
echo "Compiling GSettings schemas (requires sudo)..."
# IMPORTANT: Use umask 0022 to ensure schemas are world-readable (644)
# Without this, restrictive root umask may create 640 files that users can't read
sudo sh -c 'umask 0022 && glib-compile-schemas /usr/share/glib-2.0/schemas' 2>/dev/null || true
print_status "GSettings schemas compiled"

# Figure out helper script path (multiarch-aware)
HELPER1="/usr/lib/evolution-translate/translate/install_default_models.py"
HELPER2="/usr/lib/x86_64-linux-gnu/evolution-translate/translate/install_default_models.py"
if [ -f "$HELPER1" ]; then
    HELPER="$HELPER1"
elif [ -f "$HELPER2" ]; then
    HELPER="$HELPER2"
else
    HELPER=""
fi

echo ""
if [ -n "$HELPER" ]; then
    echo "Installing default translation models as current user..."
    "$VENV_DIR/bin/python" "$HELPER" || true
    print_status "Model installation step finished"
else
    print_warning "Could not locate install_default_models.py; skipping model install"
fi

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
echo "     - Press Ctrl+Shift+T or go to Tools → Translate Message"
echo ""
echo "  4. Configure settings:"
echo "     - Go to Edit → Preferences → Translate Settings"
echo ""
echo "Environment variables for testing (optional):"
echo "  export TRANSLATE_HELPER_PATH=\"$PROJECT_ROOT/tools/translate/translate_runner.py\""
echo "  export TRANSLATE_PYTHON_BIN=\"$VENV_DIR/bin/python\""
echo ""
echo "To manage translation models:"
echo "  $VENV_DIR/bin/python $( [ -n \"$HELPER\" ] && echo \"$HELPER\" || echo \"/usr/lib/evolution-translate/translate/install_default_models.py\" )"
echo ""
echo "To uninstall, run: $SCRIPT_DIR/uninstall.sh"
