from pathlib import Path
import tempfile,json,hashlib,struct,unittest
from flash_doom import flash_doom,ADDRESS

def partitions():
 data=b''
 for name,sub,addr,size in [('doom',0x12,0x20000,0xe0000),('os_a',0x10,0x100000,0x1b0000),('os_b',0x11,0x2b0000,0x1b0000)]:
  data+=struct.pack('<HBBII16sI',0x50aa,0,sub,addr,size,name.encode(),0)
 return data+b'\xff'*(4096-len(data))
class FlashScope(unittest.TestCase):
 def payload(self,p):
  b=bytearray(512);b[0]=0xe9;struct.pack_into('<H',b,12,9);b[80:112]=b'freslest_doom_s3'.ljust(32,b'\0')
  b+=b'FRESLEST_VIDEO:VGA25175_25175000 DOOM_VGA:PRIVATE_320x240'
  (p/'freslest_doom_s3.bin').write_bytes(b);(p/'manifest.json').write_text(json.dumps({'sha256':hashlib.sha256(b).hexdigest()}))
 def test_only_doom_is_written_after_read_only_layout_check(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);self.payload(p);calls=[]
   def cli(args):
    calls.append(args)
    if 'read_flash' in args:Path(args[-1]).write_bytes(partitions())
   flash_doom('COM7',run_cli=cli,directory=p)
   self.assertEqual(len(calls),2);self.assertIn('read_flash',calls[0]);self.assertEqual(calls[1][-4:],['write_flash','-z',hex(ADDRESS),str(p/'freslest_doom_s3.bin')])
   self.assertNotIn('erase_flash',str(calls));self.assertNotIn('erase_region',str(calls))
 def test_wrong_layout_stops_before_any_write(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);self.payload(p);calls=[]
   def cli(args):calls.append(args);Path(args[-1]).write_bytes(b'\xff'*4096)
   with self.assertRaises(ValueError):flash_doom('COM7',run_cli=cli,directory=p)
   self.assertEqual(len(calls),1);self.assertIn('read_flash',calls[0])
 def test_bad_image_stops_before_connect(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);self.payload(p);(p/'freslest_doom_s3.bin').write_bytes(b'broken');calls=[]
   with self.assertRaises(ValueError):flash_doom('COM7',run_cli=lambda a:calls.append(a),directory=p)
   self.assertEqual(calls,[])
if __name__=='__main__':unittest.main()
