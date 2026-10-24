from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 exe=Path(d)/'input';o=Path(d)/'hid.o'
 inc=['-I'+str(r/p) for p in ('OS/tests/stubs','tests/screen_stubs','shared_components/freslest_hid/include','shared_components/freslest_video/include')]
 flags=['-g','-fsanitize=address,undefined']
 subprocess.run(['gcc','-std=gnu17',*flags,*inc,'-c',str(r/'shared_components/freslest_hid/freslest_hid_state.c'),'-o',str(o)],check=True)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',*flags,*inc,str(r/'tests/test_screen_input_isolation.cpp'),str(o),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
