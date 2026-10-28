"""Reject internally valid profile payloads whose OS release is stale."""
import hashlib,json,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT));sys.path.insert(0,str(ROOT/'installer'))
import build_windows
from payload import ITEMS,DISPLAY_MODELS
class PackagedReleaseVersionTests(unittest.TestCase):
 def test_old_profile_cannot_be_packaged_as_1_1(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   for model in DISPLAY_MODELS:
    p=root/model;p.mkdir();manifest={}
    for _,name,_ in ITEMS:
     data=bytearray(80)
     if name=='freslest_os_s3.bin':data[0x30:0x50]=b'1.1'+bytes(29)
     if name in ('freslest_os_s3.bin','freslest_doom_s3.bin'):data+={'ili9488':b'FRESLEST_LCD:ILI9488','ili9341':b'FRESLEST_LCD:ILI9341','vga':b'FRESLEST_LCD:ILI9341 FRESLEST_VIDEO:VGA252_25200000'}[model]
     (p/name).write_bytes(data);manifest['firmware/'+name]=hashlib.sha256(data).hexdigest()
    (p/'SHA256.json').write_text(json.dumps(manifest))
   build_windows.verify_display_firmware(root)
   for model in DISPLAY_MODELS:
    p=root/model;image=p/'freslest_os_s3.bin';original=image.read_bytes();old=bytearray(original);old[0x30:0x50]=b'1.0'+bytes(29);image.write_bytes(old)
    manifest=json.loads((p/'SHA256.json').read_text());manifest['firmware/freslest_os_s3.bin']=hashlib.sha256(old).hexdigest();(p/'SHA256.json').write_text(json.dumps(manifest))
    with self.assertRaisesRegex(RuntimeError,'1.1'):build_windows.verify_display_firmware(root)
    image.write_bytes(original);manifest['firmware/freslest_os_s3.bin']=hashlib.sha256(original).hexdigest();(p/'SHA256.json').write_text(json.dumps(manifest))
