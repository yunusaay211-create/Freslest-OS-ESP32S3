from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[2];h=r/'shared_components/freslest_hid'
with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'hid_map'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(h/'include'),str(h/'freslest_hid_map.c'),str(h/'freslest_hid_state.c'),str(r/'OS/tests/test_hid_map.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
