from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'usb_storage'
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-g','-fsanitize=address,undefined','-I'+str(r/'stubs'),str(r/'test_usb_storage.cpp'),'-o',str(p)],check=True)
 subprocess.run([str(p)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
