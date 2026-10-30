"""Build only Doom. Never build or stage an OS, bootloader or filesystem payload."""
from pathlib import Path
import os,subprocess,sys,json,hashlib,shutil,struct
R=Path(__file__).resolve().parents[1]
def main():
 out=R/'Doom/build-vga-only'
 subprocess.run([sys.executable,str(Path(os.environ['IDF_PATH'])/'tools/idf.py'),'-B',str(out),'-DFRESLEST_LCD_MODEL=9341','-DFRESLEST_VGA_PROFILE=vga','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','build'],cwd=R/'Doom',env={**os.environ,'IDF_COMPONENT_MANAGER':'0','IDF_TARGET':'esp32s3'},check=True)
 commands=json.loads((out/'compile_commands.json').read_text())
 files=[Path(c['file']).as_posix() for c in commands]
 if not any(f.endswith('/Doom/components/freslest_video/doom_video.c') for f in files) or any(f.endswith('/shared_components/freslest_video/freslest_video.c') for f in files):raise RuntimeError('Doom private backend was not selected')
 if not any(f.endswith('/Doom/components/freslest_video/doom_rgb332.c') for f in files) or any(f.endswith('/shared_components/freslest_video/vga_doom.c') for f in files):raise RuntimeError('Doom native RGB332 word path was not selected')
 b=(out/'freslest_doom_s3.bin').read_bytes()
 assert 288<=len(b)<=0xE0000 and b[0]==0xe9 and struct.unpack_from('<H',b,12)[0]==9
 assert b[80:112].split(b'\0')[0]==b'freslest_doom_s3'
 assert b'DOOM_VGA:PRIVATE_320x240' in b and b'FRESLEST_VIDEO:VGA25175_25175000' in b
 assert b'DOOM_VGA:NATIVE_RGB332_WORDS' in b and b'DOOM_BOOT reset=' in b
 assert b'no automatic replay' in b
 target=R/'doom_updater/payload';target.mkdir(exist_ok=True)
 (target/'freslest_doom_s3.bin').write_bytes(b)
 m={'source_commit':os.environ.get('GITHUB_SHA','local'),'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'flash_address':0x20000,'partition_bytes':0xe0000,'doom_render':[320,240],'vga_signal':[640,480],'target_clock_hz':25175000,'maximum_submitted_fps':30,'os_modified':False,'nvs_modified':False,'audio_status_replay':False,'audio_first_online_confirmation_replay':False,'hardware_color_verified':False}
 (target/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
 print(json.dumps(m,indent=2))
if __name__=='__main__':main()
