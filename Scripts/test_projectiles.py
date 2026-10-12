#!/usr/bin/env python3
"""Run after test_core.py. Compare native enemy projectile collisions and execution with the original ROM interpreter."""
import os
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
cc = os.environ.get('CC') or shutil.which('clang') or shutil.which('gcc')
flags = ['-std=c11','-D_POSIX_C_SOURCE=200809L','-g','-O1','-ISources/Core','-IVendor/GBRuntime/include','-IDerivedSources','-pthread']
out = ROOT/'build/core-tests'
objects = [p for p in sorted(out.glob('*.o')) if p.stem not in ('core_test','DXGround','DXPhysics','DXCollision','DXEntity','DXActors','DXRoaming','DXProjectiles') and not p.name.endswith('.tmp.o')]
assert objects, 'Run Scripts/test_core.py first'
subprocess.run([cc,*flags,'Sources/Core/DXGround.c','Sources/Core/DXPhysics.c','Sources/Core/DXCollision.c','Sources/Core/DXEntity.c','Sources/Core/DXActors.c','Sources/Core/DXRoaming.c','Sources/Core/DXProjectiles.c','Tests/projectiles_test.c',*map(str,objects),'-lm','-o',str(out/'projectiles_test')],check=True)
with (out/'projectiles_test.log').open('w') as log:
    result = subprocess.run([str(out/'projectiles_test')],stdout=log,stderr=subprocess.PIPE,text=True,timeout=180)
print(result.stderr)
result.check_returncode()
