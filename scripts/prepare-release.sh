#!/usr/bin/env bash
#
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 Andrea Ivan Costantino
#
# Automated release preparation script
# Usage: ./scripts/prepare-release.sh <version> [--dry-run]
#
# This script automates the entire release process:
# 1. Validates the version format
# 2. Updates version in CMakeLists.txt
# 3. Updates debian/changelog
# 4. Validates version consistency
# 5. Commits changes
# 6. Creates git tag
# 7. Pushes to remote (triggering release workflow)
#

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
error() {
    echo -e "${RED}✗ Error: $1${NC}" >&2
    exit 1
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

header() {
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

# Usage information
usage() {
    cat <<EOF
Usage: $0 <version> [options]

Prepare a new release of evolution-translate-extension

Arguments:
  version         Version number (e.g., 1.1.0, 2.0.0)

Options:
  --dry-run       Show what would be done without making changes
  --no-push       Create tag but don't push to remote
  -h, --help      Show this help message

Examples:
  $0 1.1.0                    # Prepare and push release 1.1.0
  $0 1.1.0 --dry-run          # Preview changes without applying
  $0 1.1.0 --no-push          # Prepare locally but don't trigger release

Version Format:
  - Use semantic versioning: MAJOR.MINOR.PATCH
  - Minor releases only trigger GitHub release workflow (v1.0, v1.1)
  - This matches the pattern in .github/workflows/release.yml

Release Process:
  1. Updates CMakeLists.txt version
  2. Updates debian/changelog with new entry
  3. Validates version consistency
  4. Commits changes with conventional commit message
  5. Creates annotated git tag (v<version>)
  6. Pushes to remote (triggers automated release workflow)

The automated workflow will:
  - Build Debian package for Ubuntu 24.04
  - Create source tarball
  - Generate SHA256 checksums
  - Publish GitHub release with all artifacts

EOF
    exit 0
}

# Check dependencies
check_dependencies() {
    local deps=(git devscripts cmake)
    local missing=()

    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &>/dev/null; then
            missing+=("$dep")
        fi
    done

    if [ ${#missing[@]} -gt 0 ]; then
        error "Missing dependencies: ${missing[*]}\n       Install with: sudo apt install ${missing[*]}"
    fi
}

# Validate version format
validate_version() {
    local version="$1"
    if ! [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        error "Invalid version format: $version\n       Expected format: MAJOR.MINOR.PATCH (e.g., 1.1.0)"
    fi
}

# Check git repository status
check_git_status() {
    if ! git rev-parse --git-dir &>/dev/null; then
        error "Not in a git repository"
    fi

    if [ -n "$(git status --porcelain)" ]; then
        warning "Working directory has uncommitted changes"
        echo ""
        git status --short
        echo ""
        read -p "Continue anyway? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            error "Aborted by user"
        fi
    fi

    # Check if we're on main or prod-readiness
    local branch=$(git rev-parse --abbrev-ref HEAD)
    if [[ "$branch" != "main" && "$branch" != "prod-readiness" ]]; then
        warning "Not on main or prod-readiness branch (current: $branch)"
        read -p "Continue anyway? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            error "Aborted by user"
        fi
    fi
}

# Check if tag already exists
check_tag_exists() {
    local version="$1"
    local tag="v${version%.*}"  # v1.1 from 1.1.0

    if git rev-parse "$tag" &>/dev/null; then
        error "Tag $tag already exists"
    fi
}

# Update CMakeLists.txt version
update_cmake_version() {
    local version="$1"
    local file="CMakeLists.txt"

    if [ ! -f "$file" ]; then
        error "CMakeLists.txt not found"
    fi

    if [ "$DRY_RUN" = true ]; then
        info "Would update $file: VERSION \"$version\""
        return
    fi

    # Update version in CMakeLists.txt
    sed -i "s/^set(VERSION \".*\")/set(VERSION \"$version\")/" "$file"

    # Verify change
    local new_version=$(grep '^set(VERSION' "$file" | cut -d'"' -f2)
    if [ "$new_version" != "$version" ]; then
        error "Failed to update version in $file"
    fi

    success "Updated $file to version $version"
}

# Update debian/changelog
update_debian_changelog() {
    local version="$1"
    local deb_version="${version}-1"  # Debian package version includes revision

    if [ "$DRY_RUN" = true ]; then
        info "Would update debian/changelog to version $deb_version"
        return
    fi

    # Use dch (devscripts) to properly update changelog
    DEBEMAIL="${DEBEMAIL:-noreply@example.com}" \
    DEBFULLNAME="${DEBFULLNAME:-Release Manager}" \
    dch -v "$deb_version" "Release version $version"

    success "Updated debian/changelog to version $deb_version"
}

# Validate version consistency
validate_versions() {
    local expected_version="$1"

    # Check CMakeLists.txt
    local cmake_version=$(grep '^set(VERSION' CMakeLists.txt | cut -d'"' -f2)
    if [ "$cmake_version" != "$expected_version" ]; then
        error "Version mismatch in CMakeLists.txt: $cmake_version (expected: $expected_version)"
    fi

    # Check debian/changelog
    local deb_version=$(dpkg-parsechangelog -S Version | cut -d'-' -f1)
    if [ "$deb_version" != "$expected_version" ]; then
        error "Version mismatch in debian/changelog: $deb_version (expected: $expected_version)"
    fi

    success "Version consistency validated: $expected_version"
}

# Commit changes
commit_changes() {
    local version="$1"

    if [ "$DRY_RUN" = true ]; then
        info "Would commit changes with message: 'chore: release version $version'"
        return
    fi

    git add CMakeLists.txt debian/changelog
    git commit -m "chore: release version $version

- Update version to $version in CMakeLists.txt
- Update debian/changelog to ${version}-1
- Prepared for release via automated workflow
"

    success "Committed version changes"
}

# Create git tag
create_tag() {
    local version="$1"
    local tag="v${version%.*}"  # v1.1 from 1.1.0

    if [ "$DRY_RUN" = true ]; then
        info "Would create tag: $tag"
        return
    fi

    git tag -a "$tag" -m "Evolution Translation Extension $version

Release $version includes automated builds for Ubuntu 24.04.

See release notes at:
https://github.com/costantinoai/evolution-mail-translate/releases/tag/$tag
"

    success "Created tag: $tag"
    echo "$tag"  # Return tag name
}

# Push changes
push_changes() {
    local tag="$1"

    if [ "$NO_PUSH" = true ]; then
        warning "Skipping push (--no-push flag set)"
        info "To push manually, run: git push && git push origin $tag"
        return
    fi

    if [ "$DRY_RUN" = true ]; then
        info "Would push commits and tag $tag to origin"
        return
    fi

    # Push commits first
    git push

    # Push tag
    git push origin "$tag"

    success "Pushed changes and tag to origin"
}

# Main function
main() {
    local version=""
    DRY_RUN=false
    NO_PUSH=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --no-push)
                NO_PUSH=true
                shift
                ;;
            -*)
                error "Unknown option: $1\n       Use --help for usage information"
                ;;
            *)
                if [ -z "$version" ]; then
                    version="$1"
                else
                    error "Too many arguments\n       Use --help for usage information"
                fi
                shift
                ;;
        esac
    done

    # Check if version provided
    if [ -z "$version" ]; then
        error "Version argument required\n       Usage: $0 <version>\n       Example: $0 1.1.0"
    fi

    # Display banner
    header "Evolution Translation Extension - Release Preparation"

    if [ "$DRY_RUN" = true ]; then
        warning "DRY RUN MODE - No changes will be made"
        echo ""
    fi

    info "Target version: $version"
    info "Git tag will be: v${version%.*}"
    echo ""

    # Run checks
    header "Pre-flight Checks"
    check_dependencies
    validate_version "$version"
    check_git_status
    check_tag_exists "$version"
    success "All pre-flight checks passed"

    # Update files
    header "Updating Version Files"
    update_cmake_version "$version"
    update_debian_changelog "$version"

    # Validate consistency
    if [ "$DRY_RUN" = false ]; then
        validate_versions "$version"
    fi

    # Commit and tag
    header "Creating Release Commit and Tag"
    commit_changes "$version"
    tag=$(create_tag "$version")

    # Push to remote
    header "Publishing Release"
    push_changes "$tag"

    # Success summary
    header "Release Preparation Complete!"

    if [ "$DRY_RUN" = true ]; then
        info "This was a dry run. No changes were made."
        echo ""
        info "To create the release for real, run:"
        echo "  $0 $version"
    else
        success "Version $version is ready for release"
        echo ""
        if [ "$NO_PUSH" = false ]; then
            info "GitHub Actions will now build and publish the release:"
            echo "  https://github.com/costantinoai/evolution-mail-translate/actions"
            echo ""
            info "Release will be published at:"
            echo "  https://github.com/costantinoai/evolution-mail-translate/releases/tag/v${version%.*}"
        else
            info "Tag created locally but not pushed."
            info "To trigger the release workflow, run:"
            echo "  git push && git push origin v${version%.*}"
        fi
    fi

    echo ""
}

# Run main function
main "$@"
