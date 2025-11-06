#!/bin/bash
# Evolution Translation Extension - Uninstall Script
# Removes the extension from system directories (requires sudo)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# System locations
INSTALL_PREFIX="/usr"
SYSTEM_TRANSLATE_ROOT="/usr/lib/evolution-translate"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() { echo -e "${GREEN}✓${NC} $1"; }
print_error()  { echo -e "${RED}✗${NC} $1"; }
print_warning(){ echo -e "${YELLOW}⚠${NC} $1"; }

echo "Uninstalling Evolution Translation Extension..."
echo ""

# Try CMake uninstall first (if install_manifest.txt exists)
if [ -f "$BUILD_DIR/install_manifest.txt" ]; then
    echo "Using CMake uninstall (requires sudo)..."
    cd "$BUILD_DIR"
    if sudo make uninstall 2>/dev/null; then
        print_status "Extension uninstalled via CMake"
    else
        print_warning "CMake uninstall failed, proceeding with manual removal"
    fi
else
    print_warning "No install manifest found at $BUILD_DIR/install_manifest.txt"
    print_warning "Will proceed with manual removal"
fi

# Manual removal (handles both multiarch paths and any stragglers)
echo ""
echo "Removing extension files manually (requires sudo)..."

# Module can be in /usr/lib or /usr/lib/x86_64-linux-gnu
for MODULE_DIR in /usr/lib/evolution/modules /usr/lib/x86_64-linux-gnu/evolution/modules; do
    MODULE="$MODULE_DIR/libtranslate-module.so"
    if [ -f "$MODULE" ]; then
        sudo rm -f "$MODULE"
        print_status "Removed: $MODULE"
    fi
done

# Remove GSettings schema
SCHEMA_FILE="$INSTALL_PREFIX/share/glib-2.0/schemas/org.gnome.evolution.translate.gschema.xml"
if [ -f "$SCHEMA_FILE" ]; then
    sudo rm -f "$SCHEMA_FILE"
    print_status "Removed GSettings schema"

    # Recompile system schemas
    echo "Recompiling system GSettings schemas..."
    # Use umask 0022 to ensure schemas are world-readable (644)
    sudo sh -c 'umask 0022 && glib-compile-schemas /usr/share/glib-2.0/schemas' 2>/dev/null || true
    print_status "Schemas recompiled"
fi

# Ask about removing system venv and tools
echo ""
if [ -d "$SYSTEM_TRANSLATE_ROOT" ]; then
    read -p "Remove system Python environment and tools ($SYSTEM_TRANSLATE_ROOT)? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo rm -rf "$SYSTEM_TRANSLATE_ROOT"
        print_status "Removed $SYSTEM_TRANSLATE_ROOT"
    else
        print_warning "Kept $SYSTEM_TRANSLATE_ROOT (translation models remain)"
    fi
fi

# Ask about build directory
echo ""
if [ -d "$BUILD_DIR" ]; then
    read -p "Remove build directory ($BUILD_DIR)? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$BUILD_DIR"
        print_status "Removed build directory"
    fi
fi

# Note about user data
echo ""
print_warning "Translation models installed in ~/.local/share/argos-translate were NOT removed"
echo "  To remove them: rm -rf ~/.local/share/argos-translate"

# Print success message
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
print_status "Uninstallation completed!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Please restart Evolution for changes to take effect:"
echo "  killall evolution 2>/dev/null; evolution &"
echo ""
