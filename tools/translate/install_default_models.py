#!/usr/bin/env python3
"""
install_default_models.py
Installs default translation models for common languages to English.
Supports: Dutch (nl), Spanish (es), French (fr), German (de), Italian (it), Portuguese (pt), and more.
"""

import os
import sys

# Import shared GPU utilities
from gpu_utils import setup_gpu_acceleration

# Set up GPU acceleration before importing argostranslate
setup_gpu_acceleration()

def install_default_models():
    """Install default translation models for major languages to English."""
    try:
        import argostranslate.package as pkg
    except ImportError:
        print("ERROR: argostranslate not installed. Run: pip install argostranslate")
        return 1

    # Default language pairs to install (all to English)
    default_languages = [
        ("nl", "en", "Dutch"),
        ("es", "en", "Spanish"),
        ("fr", "en", "French"),
        ("de", "en", "German"),
        ("it", "en", "Italian"),
        ("pt", "en", "Portuguese"),
        ("ru", "en", "Russian"),
        ("zh", "en", "Chinese"),
        ("ja", "en", "Japanese"),
        ("ko", "en", "Korean"),
        ("ar", "en", "Arabic"),
        ("pl", "en", "Polish"),
        ("tr", "en", "Turkish"),
    ]

    print("Updating package index...")
    try:
        pkg.update_package_index()
    except Exception as e:
        print(f"ERROR: Failed to update package index: {e}")
        return 1

    available_packages = pkg.get_available_packages()
    installed_count = 0

    for from_code, to_code, language_name in default_languages:
        print(f"\n[{language_name}] Checking {from_code} → {to_code}...")

        # Check if already installed
        installed = pkg.get_installed_packages()
        if any(p.from_code == from_code and p.to_code == to_code for p in installed):
            print(f"  ✓ Already installed")
            continue

        # Find and install the package
        found = False
        for p in available_packages:
            if p.type == "translate" and p.from_code == from_code and p.to_code == to_code:
                found = True
                print(f"  Downloading...")
                try:
                    download_path = p.download()
                    print(f"  Installing...")
                    pkg.install_from_path(download_path)
                    print(f"  ✓ Successfully installed {from_code} → {to_code}")
                    installed_count += 1
                except Exception as e:
                    print(f"  ✗ Failed to install: {e}")
                break

        if not found:
            print(f"  ✗ Package not found in repository")

    print(f"\n{'='*60}")
    print(f"Installation complete! Installed {installed_count} new models.")
    print(f"{'='*60}")
    return 0


if __name__ == "__main__":
    sys.exit(install_default_models())
