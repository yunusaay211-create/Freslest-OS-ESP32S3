from pathlib import Path
import os, subprocess, tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 exe=Path(d)/'screen'; state=Path(d)/'state.o'
 inc='-I'+str(r/'shared_components/freslest_hid/include')
 subprocess.run(['gcc','-fsanitize=address,undefined','-g',inc,'-c',str(r/'shared_components/freslest_hid/freslest_hid_state.c'),'-o',str(state)],check=True)
 subprocess.run(['g++','-std=c++17','-fsanitize=address,undefined','-g','-I'+str(r/'tests/screen_stubs'),'-I'+str(r/'shared_components/freslest_video/include'),inc,str(r/'tests/test_screen_lifecycle.cpp'),str(state),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
