from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];v=r/'shared_components/freslest_av'
with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'av_engine'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-g','-O2','-fsanitize=address,undefined','-I'+str(v/'include'),str(r/'tests/test_av_engine_signal.c'),str(v/'av_signal.c'),str(v/'av_ownership.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
