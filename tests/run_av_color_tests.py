from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1];v=r/'shared_components/freslest_video'
with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'av_color'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(v/'include'),'-I'+str(r/'shared_components/freslest_av/include'),str(r/'tests/test_av_colors.c'),str(v/'video_pixels.c'),str(r/'shared_components/freslest_av/av_signal.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
