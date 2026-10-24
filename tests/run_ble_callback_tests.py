"""Compile complete BLE lifecycle with real HID parser/state."""
from pathlib import Path
import os, subprocess, tempfile
r=Path(__file__).resolve().parents[1];h=r/'shared_components/freslest_hid'
with tempfile.TemporaryDirectory() as d:
 d=Path(d);objects=[];flags=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(h/'include')]
 for name in ['freslest_hid_map.c','freslest_hid_state.c']:
  obj=d/(name+'.o');objects.append(str(obj));subprocess.run(['gcc','-std=gnu17',*flags,'-c',str(h/name),'-o',str(obj)],check=True)
 exe=d/'ble';subprocess.run(['g++','-std=c++17',*flags,'-I'+str(r/'tests/ble_stubs'),str(r/'tests/test_ble_lifecycle.cpp'),*objects,'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
