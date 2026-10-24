"""Exercise the production ZIP/parser/image validator with hostile ZIP fixtures.

Host decompression/SHA backends are zlib/OpenSSL; firmware uses S3 ROM tinfl and
mbedTLS. Parser, sizes, manifests, CRC and image validation are shared C code.
Run with IDF_PATH pointing at the ESP-IDF v5.4.4 checkout (for cJSON).
"""
from pathlib import Path
import copy,hashlib,io,json,os,random,struct,subprocess,tempfile,zipfile
root=Path(__file__).resolve().parents[1]
shared=root.parent/'common/usb_update'
idf=Path(os.environ.get('IDF_PATH',''))
cjson=idf/'components/json/cJSON'
if not (cjson/'cJSON.c').exists():raise SystemExit('Set IDF_PATH to ESP-IDF v5.4.4')
build=root/'tests/host_build';build.mkdir(exist_ok=True)
probe=build/'update_probe'
subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',
 '-DFRESLEST_UPDATE_HOST_TEST','-I'+str(shared),'-I'+str(cjson),
 str(shared/'freslest_update_package.c'),str(root/'tests/update_probe.c'),str(cjson/'cJSON.c'),
 '-lcrypto','-lz','-lm','-o',str(probe)],check=True)
def image():
 data=bytearray(random.Random(537).randbytes(131071))
 data[:256]=bytes(256);struct.pack_into('<I',data,0,0xabcd5432)
 data[16:22]=b'4.0.0\0';data[48:63]=b'freslest_os_s3\0'
 h=bytearray(24);h[0:4]=bytes([0xe9,1,2,0x40]);struct.pack_into('<H',h,12,9);h[23]=1
 b=h+struct.pack('<II',0x3c020020,len(data))+data
 checksum=0xef
 for c in data:checksum^=c
 while (len(b)+1)%16:b.append(0)
 b.append(checksum);return bytes(b)+hashlib.sha256(b).digest()
firmware=image()
def manifest(data=firmware):
 return {'format':'freslest-usb-update','format_version':2,'updater_min':2,
 'board':'esp32s3-n16r8-ili9488-buttons','layout':'freslest-s3-ab-v2','version':data[48:80].split(b'\0',1)[0].decode('ascii'),
 'image':{'file':'firmware/freslest_os_s3.bin','size':len(data),'sha256':hashlib.sha256(data).hexdigest()}}
def archive(data=firmware,m=None,method=zipfile.ZIP_DEFLATED,prefix='',comment=b'',extra=None):
 out=io.BytesIO()
 with zipfile.ZipFile(out,'w',compression=method) as z:
  z.writestr(prefix+'freslest-update.json',json.dumps(m or manifest(data)) if not isinstance(m,str) else m)
  z.writestr(prefix+'firmware/freslest_os_s3.bin',data)
  if extra:
   for name,content in extra:z.writestr(prefix+name,content)
  z.comment=comment
 return out.getvalue()
checks=0
with tempfile.TemporaryDirectory() as tmp:
 tmp=Path(tmp)
 def check(name,data,expected=0):
  global checks
  path=tmp/(name+'.zip');path.write_bytes(data)
  r=subprocess.run([str(probe),str(path)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  if r.returncode!=expected:raise AssertionError((name,r.returncode,r.stdout,r.stderr))
  checks+=1
 check('deflate',archive())
 check('stored',archive(method=zipfile.ZIP_STORED))
 check('project_zip',archive(prefix='Freslest-OS-Doom-S3/',extra=[('src/'+('a'*200),b'ignored'),('README.md',b'project')]))
 check('comment',archive(comment=b'PK\x05\x06'*60))
 for field,value in [('format','other'),('board','esp32'),('layout','other'),('format_version',1),('updater_min',3),('version','bad\ntext')]:
  m=manifest();m[field]=value;check('wrong_'+field,archive(m=m),2)
 for field,value in [('size',0x1b0001),('size',300.5),('file','../factory.bin'),('sha256','z'*64)]:
  m=manifest();m['image'][field]=value;check('bad_image_'+field,archive(m=m),2)
 check('duplicate_key',archive(m=json.dumps(manifest())[:-1]+',"board":"esp32s3-n16r8-ili9488-buttons"}'),2)
 check('deep_json',archive(m=json.dumps(manifest())[:-1]+',"extra":'+('['*25)+'0'+(']'*25)+'}'),2)
 check('escaped_nul',archive(m=json.dumps(manifest()).replace('4.0.0','4.0.0\\u0000hidden')),2)
 check('duplicate_manifest',archive(extra=[('freslest-update.json',json.dumps(manifest()).encode())]),2)
 check('duplicate_image',archive(extra=[('firmware/freslest_os_s3.bin',firmware)]),2)
 check('traversal',archive(prefix='../'),2)
 check('unsupported_compression',archive(method=zipfile.ZIP_BZIP2),2)
 check('truncated_zip',archive()[:-10],2)
 m=manifest();m['image']['sha256']='0'*64;check('bad_sha',archive(m=m),3)
 m=manifest();m['version']='4.0.1';check('different_binary_version',archive(m=m),3)
 for name,offset,value in [('wrong_chip',12,0),('wrong_project',80,ord('X')),('wrong_flash',3,0x30),('wrong_magic',0,0),('wrong_segment_count',1,17),('bad_esp_checksum',len(firmware)-33,0),('bad_esp_hash',len(firmware)-1,0)]:
  b=bytearray(firmware);b[offset]^=value if name=='bad_esp_hash' else 0
  b[offset]=value if name!='bad_esp_hash' else b[offset]^1
  check(name,archive(data=bytes(b)),3)
 b=bytearray(firmware);struct.pack_into('<I',b,28,0xfffffffc);check('segment_overflow',archive(data=bytes(b)),3)
 b=bytearray(archive(method=zipfile.ZIP_STORED))
 z=zipfile.ZipFile(io.BytesIO(b));info=z.getinfo('firmware/freslest_os_s3.bin')
 at=info.header_offset;offset=at+30+len(info.filename.encode())+len(info.extra);b[offset+300]^=0x55
 check('zip_crc_mismatch',bytes(b),3)
 b=bytearray(archive());central=b.find(b'PK\x01\x02');struct.pack_into('<I',b,central+42,0x7ffffff0);check('local_offset_overflow',bytes(b),2)
 b=bytearray(archive());struct.pack_into('<H',b,len(b)-22+4,1);check('split_zip',bytes(b),2)
 b=bytearray(archive());struct.pack_into('<I',b,len(b)-22+16,0xffffffff);check('zip64_offset',bytes(b),2)
 if (root.parent/'OS/build/freslest_os_s3.bin').is_file():
  actual=(root.parent/'OS/build/freslest_os_s3.bin').read_bytes()
  # Only run this fixture when the new version has already been cross-built.
  if actual[80:112].split(b'\0',1)[0]==b'freslest_os_s3':check('actual_cross_built_OS',archive(data=actual))
 runtime=build/'test_update_runtime'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-format-truncation','-Wno-unused-variable','-Wno-deprecated-declarations','-g','-fsanitize=address,undefined',
  '-DFRESLEST_UPDATE_HOST_TEST','-DFRESLEST_OTA_HOST_TEST','-I'+str(root.parent/'shared_components/freslest_ota/include'),'-I'+str(shared),'-I'+str(root/'tests'),
  '-I'+str(root/'components/prboom-esp32-compat/include'),'-I'+str(cjson),
  str(shared/'freslest_update_package.c'),str(root/'tests/test_update_runtime.c'),str(cjson/'cJSON.c'),
  '-lcrypto','-lz','-lm','-o',str(runtime)],check=True)
 (tmp/'usb').mkdir();(tmp/'firmware.bin').write_bytes(firmware);(tmp/'usb/update.zip').write_bytes(archive())
 m=manifest();m['image']['sha256']='0'*64;(tmp/'usb/bad-sha.zip').write_bytes(archive(m=m))
 subprocess.run([str(runtime),str(tmp/'firmware.bin')],cwd=tmp,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 # Preserve only reproducible UI captures as intermediate test outputs.
 for frame in tmp.glob('*.raw'):(build/frame.name).write_bytes(frame.read_bytes())
print(f'PASS: {checks} production update ZIP/manifest/CRC/SHA/ESP image cases; monotonic bounded progress and output guards')
