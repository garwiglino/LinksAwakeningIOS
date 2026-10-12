#!/usr/bin/env python3
"""Xcode preflight: prepare the repository's one ROM when needed, then verify."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
manifest_path = ROOT / "DerivedSources/manifest.json"
roms = sorted(ROOT.glob("*.gbc"))
try:
    manifest = json.loads(manifest_path.read_text())
except (OSError, ValueError):
    manifest = None
fixture = manifest and manifest.get("fixture") and os.environ.get("DX_ALLOW_FIXTURE") == "YES"
if not fixture:
    if len(roms) > 1:
        sys.exit("error: Plusieurs ROM .gbc à la racine. Gardez celle à compiler ou préparez-la dans un clone sans ROM à la racine.")
    if roms:
        digest = hashlib.sha256(roms[0].read_bytes()).hexdigest()
        if not manifest or manifest.get("rom_sha256") != digest:
            subprocess.run([sys.executable, str(ROOT / "Scripts/prepare_rom.py"), str(roms[0])], check=True)
subprocess.run([sys.executable, str(ROOT / "Scripts/check_generated.py")], check=True)
