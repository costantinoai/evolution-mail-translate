#!/bin/bash
# Evolution Translation Extension - Debian Package Build Script
# Builds a .deb package for distribution

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

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

# Check for required build tools
echo "Checking build dependencies..."

MISSING_DEPS=()

if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    MISSING_DEPS+=("devscripts")
fi

if ! command -v dh >/dev/null 2>&1; then
    MISSING_DEPS+=("debhelper")
fi

if ! command -v cmake >/dev/null 2>&1; then
    MISSING_DEPS+=("cmake")
fi

if ! pkg-config --exists evolution-shell-3.0 2>/dev/null; then
    MISSING_DEPS+=("evolution-dev" "evolution-data-server-dev")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    print_error "Missing build dependencies: ${MISSING_DEPS[*]}"
    echo ""
    echo "Install with:"
    echo "  sudo apt install ${MISSING_DEPS[*]}"
    exit 1
fi

print_status "All build dependencies satisfied"

# Check if debian/ directory exists
if [ ! -d "$PROJECT_ROOT/debian" ]; then
    print_error "debian/ directory not found"
    echo "  Make sure you're running this from the project root"
    exit 1
fi

# Clean previous builds
echo ""
echo "Cleaning previous builds..."
cd "$PROJECT_ROOT"
rm -rf build _build
rm -f ../*.deb ../*.changes ../*.buildinfo ../*.dsc ../*.tar.* 2>/dev/null || true
print_status "Cleaned"

# Build the package
echo ""
echo "Building Debian package..."
dpkg-buildpackage -us -uc -b

print_status "Package built successfully"

# Find the generated .deb file
DEB_FILE=$(ls -t ../*.deb 2>/dev/null | head -n1)

if [ -z "$DEB_FILE" ]; then
    print_error "No .deb file found after build"
    exit 1
fi

# Get package info
PACKAGE_NAME=$(dpkg-deb -f "$DEB_FILE" Package)
PACKAGE_VERSION=$(dpkg-deb -f "$DEB_FILE" Version)
PACKAGE_ARCH=$(dpkg-deb -f "$DEB_FILE" Architecture)

# Run lintian checks
echo ""
echo "Running package quality checks..."
if command -v lintian >/dev/null 2>&1; then
    lintian "$DEB_FILE" || print_warning "Some lintian warnings found (non-critical)"
else
    print_warning "lintian not installed, skipping quality checks"
    echo "  Install with: sudo apt install lintian"
fi

# Print success message
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
print_status "Debian package created successfully!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Package details:"
echo "  Name:     $PACKAGE_NAME"
echo "  Version:  $PACKAGE_VERSION"
echo "  Arch:     $PACKAGE_ARCH"
echo "  File:     $(basename "$DEB_FILE")"
echo ""
echo "Installation:"
echo "  sudo dpkg -i $DEB_FILE"
echo "  sudo apt-get install -f  # Fix any missing dependencies"
echo ""
echo "Testing:"
echo "  1. Install the package (see above)"
echo "  2. Restart Evolution: killall evolution && evolution"
echo "  3. Test translation on an email message"
echo ""
echo "Distribution:"
echo "  - Upload to GitHub Releases"
echo "  - Users can download and install with dpkg"
echo ""
echo "Uninstallation:"
echo "  sudo apt remove $PACKAGE_NAME"
echo ""
