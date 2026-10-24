"""Compile and execute the production recovery/sleep code with SDK doubles."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 exe=Path(d)/'power_recovery'
 subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(root/'tests/safety_stubs'),str(root/'tests/test_power_recovery.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
