#!/usr/bin/env python3
"""Validate a local LADX ROM and generate native C; standard-library only.

The pinned recompiler is built locally with the host C++ compiler. No network,
Homebrew, CMake, CocoaPods or copyrighted downloads are used.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
KNOWN = {
    "07c211479386825042efb4ad31bb525f": "English 1.0",
    "ccbb56212e3dbaa9007d389a17e9d075": "English 1.1",
    "7351daa3c0a91d8f6fe2fbcca6182478": "English 1.2",
    "1043fd167d0ed9c4094e3c9d8e757f1e": "French 1.0",
    "68242187b65166b5f8225b20e2021659": "French 1.1",
    "e91fd46e7092d32ca264f21853f09539": "German 1.0",
    "b0080c2f1919a4bb0ea73b788f4a6786": "German 1.1",
    "f75874e3654360094fc2b09bd1fed7e8": "Japanese 1.0",
    "6d8f9cd72201caabdfd0455a819af9ce": "Japanese 1.1",
    "2e2596c008d47df901394d28f5bd66ec": "Japanese 1.2",
}
PARTS = 32


def validate_rom(data, fixture=False):
    if fixture:
        # Only our generated, Nintendo-free fixture is accepted through this path.
        sys.path.insert(0, str(ROOT / "Tests"))
        from make_fixture import make_rom
        if data != make_rom():
            raise ValueError("--fixture n’accepte que la ROM de test créée par Tests/make_fixture.py")
        return "Synthetic test ROM (not Link's Awakening)", True
    if len(data) != 1024 * 1024:
        raise ValueError("Link’s Awakening DX doit faire exactement 1 048 576 octets (ROM .gbc décompressée).")
    if not data[0x143] & 0x80:
        raise ValueError("Cette ROM ne déclare pas la compatibilité Game Boy Color.")
    check = 0
    for byte in data[0x134:0x14d]:
        check = (check - byte - 1) & 255
    if check != data[0x14d]:
        raise ValueError("L’en-tête ROM est endommagé.")
    digest = hashlib.md5(data).hexdigest()
    if digest not in KNOWN:
        raise ValueError("ROM non reconnue : fournissez un dump DX non modifié (FR, EN, DE ou JP). MD5=" + digest)
    return KNOWN[digest], False


def build_recompiler():
    compiler = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise ValueError("Compilateur C++ absent. Sur Mac, installez Xcode puis xcode-select --install.")
    source = ROOT / "Vendor/Recompiler"
    files = sorted(source.rglob("*.cpp"))
    signature = hashlib.sha256()
    signature.update(compiler.encode())
    for file in sorted(source.rglob("*")):
        if file.is_file():
            signature.update(file.relative_to(source).as_posix().encode())
            signature.update(file.read_bytes())
    binary = ROOT / "build/gbrecomp"
    stamp = ROOT / "build/recompiler.sha256"
    if binary.exists() and stamp.exists() and stamp.read_text() == signature.hexdigest():
        return binary
    binary.parent.mkdir(exist_ok=True)
    subprocess.run([compiler, "-std=c++20", "-O2", "-I" + str(source / "include"),
                    *map(str, files), "-o", str(binary)], check=True)
    stamp.write_text(signature.hexdigest())
    return binary


def split_source(source, output):
    """Bound compiler memory using fixed translation units with shared declarations.

    Pinned emitter uses column-zero closing braces. Fail if a future emitter
    changes that contract instead of silently dropping any generated code.
    """
    pattern = re.compile(r"^(?:__attribute__\(\(noinline\)\) )?(?:static )?void \w+\([^\n]*\) \{\n.*?^\}\n", re.M | re.S)
    matches = list(pattern.finditer(source))
    if not matches:
        raise ValueError("Aucune fonction recompilée détectée.")
    outside = pattern.sub("", source)
    outside = re.sub(r"/\*.*?\*/", "", outside, flags=re.S)
    outside = re.sub(r"^#.*$", "", outside, flags=re.M)
    outside = re.sub(r"(?:static )?void \w+\([^;]*\);", "", outside)
    outside = re.sub(r"extern const uint8_t rom_data\[\];", "", outside)
    if outside.strip():
        raise ValueError("Format du recompileur inattendu : fractionnement refusé.")
    functions = [re.sub(r"\bstatic void\b", "void", m.group(0), count=1) for m in matches]
    declarations = [body.split(" {\n", 1)[0] + ";" for body in functions]
    common = '#pragma once\n#include "rom.h"\n#include <stdio.h>\n#include <stdlib.h>\nextern const uint8_t rom_data[];\n'
    (output / "rom_functions.h").write_text(common + "\n".join(declarations) + "\n")
    groups = [[] for _ in range(PARTS)]
    sizes = [0] * PARTS
    for body in functions:
        i = min(range(PARTS), key=lambda n: sizes[n])
        groups[i].append(body)
        sizes[i] += len(body)
    for i, group in enumerate(groups):
        (output / f"rom_part_{i:02d}.c").write_text('#include "rom_functions.h"\n' + "\n".join(group))
    return len(functions)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("--fixture", action="store_true", help="CI only: exact generated homebrew fixture")
    args = parser.parse_args()
    if args.rom.stat().st_size > 1024 * 1024:
        raise ValueError("Fichier trop volumineux : une ROM DX décompressée est attendue.")
    data = args.rom.read_bytes()
    revision, is_fixture = validate_rom(data, args.fixture)
    print("ROM validée :", revision, flush=True)
    binary = build_recompiler()
    with tempfile.TemporaryDirectory(prefix="dx-gen-", dir=ROOT / "build") as temporary:
        stage = Path(temporary)
        rom = stage / "rom.gbc"  # fixes generated symbol names, independent of original filename
        rom.write_bytes(data)
        generated = stage / "generated"
        with (ROOT / "build/recompile.log").open("w") as log:
            subprocess.run([str(binary), str(rom), "-o", str(generated)], stdout=log, stderr=subprocess.STDOUT, check=True)
        output = stage / "output"
        output.mkdir()
        count = split_source((generated / "rom.c").read_text(), output)
        for name in ("rom.h", "rom_rom.c"):
            shutil.copy2(generated / name, output / name)
        sha = hashlib.sha256(data).hexdigest()
        (output / "GameIdentity.h").write_text(f'#pragma once\n#define DX_ROM_SHA256 "{sha}"\n#define DX_ROM_SIZE {len(data)}u\n#define DX_IS_FIXTURE {int(is_fixture)}\n')
        manifest = {"schema": 1, "rom_sha256": sha, "rom_md5": hashlib.md5(data).hexdigest(),
                    "revision": revision, "fixture": is_fixture, "functions": count,
                    "upstream": "ceb856bb66dcf0d839054148b9f96f49d4fe23b8"}
        manifest["files"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.iterdir())}
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        destination = ROOT / "DerivedSources"
        destination.mkdir(exist_ok=True)
        # Manifest is installed last. An interrupted preparation fails preflight,
        # never silently links old code to a different ROM.
        (destination / "manifest.json").unlink(missing_ok=True)
        for p in output.iterdir():
            if p.name != "manifest.json":
                os.replace(p, destination / p.name)
        os.replace(output / "manifest.json", destination / "manifest.json")
    print(f"Prêt : {count} fonctions, {PARTS} unités C. Ouvrez LinksAwakeningIOS.xcodeproj.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(f"Préparation interrompue : {error}\nDétails éventuels : build/recompile.log")
