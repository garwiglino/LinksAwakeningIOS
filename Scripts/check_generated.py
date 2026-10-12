#!/usr/bin/env python3
import hashlib
import json
import os
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
try:
    manifest = json.loads((root / "DerivedSources/manifest.json").read_text())
    if manifest.get("schema") != 1:
        raise ValueError("Version de préparation inconnue.")
    if manifest.get("fixture") and os.environ.get("DX_ALLOW_FIXTURE") != "YES":
        raise ValueError("Sources de test détectées. Préparez votre propre ROM DX.")
    required = {"rom.h", "rom_rom.c", "GameIdentity.h", "rom_functions.h"}
    required.update(f"rom_part_{i:02d}.c" for i in range(32))
    if set(manifest["files"]) != required:
        raise ValueError("Liste de sources générées incohérente.")
    for name, digest in manifest["files"].items():
        if hashlib.sha256((root / "DerivedSources" / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"Source modifiée ou incomplète : {name}")
except (OSError, ValueError, KeyError) as error:
    sys.exit(f"error: {error}\nExécutez : python3 Scripts/prepare_rom.py /chemin/jeu.gbc")
print("Sources recompilées vérifiées :", manifest["revision"])
