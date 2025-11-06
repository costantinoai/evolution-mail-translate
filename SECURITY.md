# Security Policy

## Reporting a Vulnerability

Please report suspected security issues privately via GitHub Security Advisories or by email to the maintainer.

Provide as much detail as possible, including:
- Affected versions and environment
- Steps to reproduce
- Impact assessment

We aim to triage within 7 days.

## Hardening

- Helper execution uses argument arrays (no shell).
- No PATH fallback for helper; explicit paths only.
- Sensitive path info logged only at debug level.
- Python dependencies installed in per-user venv via setup tool.

