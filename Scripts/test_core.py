#!/usr/bin/env python3
"""Compile and exercise the prepared ROM on macOS/Linux; no GUI or SDL."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
compiler = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
assert compiler, "C compiler required"
out = ROOT / "build/core-tests"
out.mkdir(parents=True, exist_ok=True)
flags = ["-std=c11", "-D_POSIX_C_SOURCE=200809L", "-g", "-O1", "-fno-inline", "-fno-strict-aliasing",
         "-ISources/Core", "-IVendor/GBRuntime/include", "-IDerivedSources", "-pthread"]
if os.environ.get("DX_SANITIZE") == "YES":
    flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
sources = sorted((ROOT / "Vendor/GBRuntime/src").glob("*.c")) + sorted((ROOT / "Sources/Core").glob("*.c")) + [ROOT / "Tests/core_test.c"]
sources += sorted((ROOT / "DerivedSources").glob("rom_part_*.c")) + [ROOT / "DerivedSources/rom_rom.c"]
headers = b"".join(p.read_bytes() for folder in ["Vendor/GBRuntime/include", "Sources/Core", "DerivedSources"] for p in sorted((ROOT/folder).glob("*.h")))
def compile_one(source):
    output = out / (source.stem + ".o")
    stamp = output.with_suffix(".sha256")
    digest = hashlib.sha256(source.read_bytes()+headers+str(flags).encode()+compiler.encode()).hexdigest()
    if not output.exists() or output.stat().st_size == 0 or not stamp.exists() or stamp.read_text() != digest:
        log = output.with_suffix(".log")
        temporary = output.with_suffix(".tmp.o")
        with log.open("w") as f:
            result = subprocess.run([compiler,*flags,"-c",str(source),"-o",str(temporary)],stdout=f,stderr=subprocess.STDOUT)
        if result.returncode:
            print(log.read_text()); result.check_returncode()
        assert temporary.stat().st_size > 0, f"Empty compiler output: {source}"
        temporary.replace(output)
        stamp.write_text(digest)
    return output
with ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
    objects = list(pool.map(compile_one, sources))
binary = out / "core_test"
subprocess.run([compiler,*flags,*map(str,objects),"-lm","-o",str(binary)],check=True)
with tempfile.TemporaryDirectory(prefix="dx-saves-", dir=out) as saves:
    subprocess.run([str(binary),saves],check=True,timeout=120)
    shutil.copy2(Path(saves)/"frame.ppm",out/"frame.ppm")
print("Core verification complete; final frame: build/core-tests/frame.ppm")
