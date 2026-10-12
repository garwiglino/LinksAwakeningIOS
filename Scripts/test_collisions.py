#!/usr/bin/env python3
"""Run after test_core.py. Compare native collision rules with the original ROM/PPU."""
import os
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
cc = os.environ.get('CC') or shutil.which('clang') or shutil.which('gcc')
flags = ['-std=c11','-D_POSIX_C_SOURCE=200809L','-g','-O1','-ISources/Core','-IVendor/GBRuntime/include','-IDerivedSources','-pthread']
out = ROOT/'build/core-tests'
objects = [p for p in sorted(out.glob('*.o')) if p.stem not in ('core_test','DXCollision','DXEntity') and not p.name.endswith('.tmp.o')]
assert objects, 'Run Scripts/test_core.py first'
subprocess.run([cc,*flags,'Sources/Core/DXCollision.c','Sources/Core/DXEntity.c','Tests/collision_test.c',*map(str,objects),'-lm','-o',str(out/'collision_test')],check=True)
with (out/'collision_test.log').open('w') as log:
    result = subprocess.run([str(out/'collision_test')],stdout=log,stderr=subprocess.PIPE,text=True,timeout=180)
print(result.stderr)
result.check_returncode()
