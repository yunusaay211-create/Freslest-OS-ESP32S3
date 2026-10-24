from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1];v=r/'shared_components/freslest_video'
with tempfile.TemporaryDirectory() as tmp:
 exe=Path(tmp)/'ntsc_timing'
 flags=['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(v/'include'),'-I'+str(r/'shared_components/freslest_av/include'),str(r/'tests/test_ntsc_timing.c'),str(r/'shared_components/freslest_av/av_signal.c')]
 env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
 subprocess.run([*flags,str(v/'video_pixels.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=env)
 # Negative controls prove that correcting only timing or only burst leaves
 # a detectable mismatch. Neither damaged variant is included in firmware.
 source=(v/'video_pixels.c').read_text()
 cases=[('(i+y*VIDEO_LINE_SAMPLES)&3','i&3','burst'),
        ('((y+VIDEO_IMAGE_TOP)*VIDEO_LINE_SAMPLES+VIDEO_IMAGE_START)&3','0','active chroma')]
 for old,new,label in cases:
  assert source.count(old)==(1 if label=='burst' else 2)
  broken=Path(tmp)/'broken.c';broken.write_text(source.replace(old,new))
  subprocess.run([*flags,str(broken),'-lm','-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True,text=True,env=env)
  assert result.returncode==-6 and 'Assertion' in result.stderr,(label,result.returncode,result.stderr)
 print('PASS: receiver model rejects both non-alternating burst and non-alternating active chroma negative controls')
