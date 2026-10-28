from pathlib import Path
import sys,subprocess,json,hashlib,struct,platform
from flash_doom import image_path
R=Path(__file__).resolve().parents[1]
if sys.platform!='win32':raise SystemExit('Build this EXE on Windows.')
image_path(R/'doom_updater/payload')
assert {p.name for p in (R/'doom_updater/payload').iterdir()}=={'freslest_doom_s3.bin','manifest.json'}
subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onefile','--windowed','--noupx','--name','Freslest Doom VGA','--collect-all','esptool','--collect-all','serial','--add-data','doom_updater/payload;payload','doom_updater/app.py'],cwd=R,check=True)
p=R/'dist/Freslest Doom VGA.exe';b=p.read_bytes();pe=struct.unpack_from('<I',b,60)[0]
assert b[:2]==b'MZ' and b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,pe+4)[0]==0x8664
assert struct.unpack_from('<H',b,pe+92)[0]==2
report={'application':'Freslest Doom VGA','version':'1.1','build_os':platform.platform(),'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'firmware':json.loads((R/'doom_updater/payload/manifest.json').read_text()),'write_addresses':[0x20000],'os_packaged':False}
(R/'dist/DOOM_BUILD_REPORT.json').write_text(json.dumps(report,indent=2)+'\n')
