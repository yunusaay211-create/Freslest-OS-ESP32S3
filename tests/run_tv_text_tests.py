from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1];v=r/'shared_components/freslest_video'
with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'tv_text'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-O2','-fsanitize=address,undefined','-I'+str(v/'include'),'-I'+str(r/'shared_components/freslest_av/include'),'-I'+str(r/'OS/components/LovyanGFX/src/lgfx/Fonts'),str(r/'tests/test_tv_text.c'),str(v/'video_pixels.c'),str(r/'shared_components/freslest_av/av_signal.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
