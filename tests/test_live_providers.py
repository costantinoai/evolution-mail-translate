# SPDX-License-Identifier: LGPL-2.1-or-later
# Copyright (C) 2026 Andrea Costantino. Added on 2026-11-24.
"""Opt-in checks that send a fixed synthetic sentence to real online services."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.skipif(os.environ.get("TRANSLATE_LIVE_PROVIDERS") != "1",
                    reason="Set TRANSLATE_LIVE_PROVIDERS=1 to contact online services")
@pytest.mark.parametrize("provider", ["mymemory", "libre"])
def test_live_provider_translates_synthetic_text(provider):
    result = subprocess.run(
        [os.environ.get("TRANSLATE_PYTHON_BIN", sys.executable),
         str(ROOT / "tools/translate/translate_runner_online.py"),
         "--provider", provider, "--source", "fr", "--target", "en", "--text"],
        input="Bonjour, ceci est un test.", capture_output=True, text=True, timeout=90)
    # The helper normalizes errors without echoing credentials or request bodies.
    payload = json.loads(result.stdout)
    assert result.returncode == 0, payload.get("error", "Provider helper failed")
    assert "error" not in payload, payload["error"]
    translated = payload.get("translated")
    assert isinstance(translated, str) and re.search(r"\bthis is a test\b", translated, re.I), \
        "The service did not return the expected English translation"
