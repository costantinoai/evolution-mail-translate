import os
from pathlib import Path
import sys

os.environ.setdefault("ARGOS_DEVICE_TYPE", "cpu")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/translate"))
