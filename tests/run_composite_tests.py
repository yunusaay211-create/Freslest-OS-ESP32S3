from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
video=root/'shared_components/freslest_video'
with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'composite'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(video/'include'),'-I'+str(root/'shared_components/freslest_av/include'),str(root/'tests/test_composite.c'),str(video/'video_pixels.c'),str(video/'video_slots.c'),str(root/'shared_components/freslest_av/av_signal.c'),str(root/'shared_components/freslest_av/av_ownership.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'backend'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-g','-fsanitize=address,undefined','-I'+str(root/'tests/video_stubs'),'-I'+str(video/'include'),'-I'+str(root/'shared_components/freslest_av/include'),str(root/'tests/test_video_backend.c'),str(video/'video_pixels.c'),str(video/'video_slots.c'),str(root/'shared_components/freslest_av/av_signal.c'),str(root/'shared_components/freslest_av/av_ownership.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
