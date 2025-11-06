#!/usr/bin/env python3
"""
setup_models.py
Utility to list and install Argos Translate models.
"""

import argparse
import sys

def list_available():
    try:
        import argostranslate.package as pkg
        pkg.update_package_index()
        for p in pkg.get_available_packages():
            if p.type == "translate":
                print(f"{p.from_code}->{p.to_code}: {p.link}")
    except Exception as e:
        print(f"Error: {e}. Did you install 'argostranslate'?", file=sys.stderr)
        return 1
    return 0

def install_pair(fr: str, to: str):
    try:
        import argostranslate.package as pkg
        pkg.update_package_index()
        for p in pkg.get_available_packages():
            if p.type == "translate" and p.from_code == fr and p.to_code == to:
                print(f"Downloading {fr}->{to}…")
                path = p.download()
                print("Installing…")
                pkg.install_from_path(path)
                print("Done.")
                return 0
        print(f"Package {fr}->{to} not found.")
        return 2
    except Exception as e:
        print(f"Error: {e}. Did you install 'argostranslate'?", file=sys.stderr)
        return 1

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true", help="List available models")
    ap.add_argument("--install", nargs=2, metavar=("FROM", "TO"), help="Install model for FROM TO")
    args = ap.parse_args()
    if args.list:
        sys.exit(list_available())
    elif args.install:
        fr, to = args.install
        sys.exit(install_pair(fr, to))
    else:
        ap.print_help()
        return 0

if __name__ == "__main__":
    sys.exit(main())
