"""Rebuild the initial LittleFS image (optional host dependency: littlefs-python 0.19.0)."""
from pathlib import Path
import hashlib,json
from littlefs import LittleFS
root=Path(__file__).resolve().parents[1]
size=0x50000
fs=LittleFS(block_size=4096,block_count=size//4096,read_size=128,prog_size=128,cache_size=512,lookahead_size=128,name_max=63,disk_version=0x00020001)
source=root/'OS/freslest/data'
for path in sorted(source.rglob('*')):
 name='/'+path.relative_to(source).as_posix()
 if path.is_dir():fs.makedirs(name,exist_ok=True)
 else:
  if any(len(part.encode())>63 for part in path.relative_to(source).parts):raise ValueError('Filename exceeds LittleFS 63-byte limit: '+name)
  with fs.open(name,'wb') as out:out.write(path.read_bytes())
fs.unmount()
image=bytes(fs.context.buffer)
assert len(image)==size
out=root/'firmware/littlefs.bin';out.write_bytes(image)
# Remount the produced bytes and check every file before publishing the image.
from littlefs.context import UserContext
check=LittleFS(context=UserContext(buffer=bytearray(image)),block_size=4096,block_count=size//4096,read_size=128,prog_size=128,cache_size=512,lookahead_size=128,name_max=63,disk_version=0x00020001,mount=True)
for path in source.rglob('*'):
 if path.is_file():
  with check.open('/'+path.relative_to(source).as_posix(),'rb') as f:assert f.read()==path.read_bytes()
check.unmount()
print('LittleFS image verified:',len(image),'bytes, SHA256',hashlib.sha256(image).hexdigest())

manifest_path=root/'firmware/SHA256.json'
if manifest_path.exists():
 data=json.loads(manifest_path.read_text());data['firmware/littlefs.bin']=hashlib.sha256(image).hexdigest();manifest_path.write_text(json.dumps(data,indent=2)+'\n')
