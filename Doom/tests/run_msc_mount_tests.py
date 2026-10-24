"""Test production MSC/FatFS using sparse USB images; no hardware is simulated."""
from pathlib import Path
import os,struct,subprocess,tempfile,zlib
root=Path(__file__).resolve().parents[1];idf=Path(os.environ['IDF_PATH']);msc=root.parent/'shared_components/usb_host_msc'
build=root/'tests/host_build';build.mkdir(exist_ok=True)
inc=[root/'tests/msc_stubs',root.parent/'OS/build/config',msc/'private_include',idf/'components/fatfs/src',idf/'components/fatfs/diskio',idf/'components/esp_common/include']
exe=build/'test_msc_mount'
subprocess.run(['gcc','-std=gnu17','-g','-O1','-fsanitize=address,undefined','-Wno-discarded-qualifiers','-include','assert.h',*['-I'+str(p) for p in inc],str(root/'tests/test_msc_mount.c'),str(msc/'src/diskio_usb.c'),str(msc/'src/msc_host_vfs.c'),str(idf/'components/fatfs/src/ff.c'),str(idf/'components/fatfs/src/ffunicode.c'),str(idf/'components/fatfs/diskio/diskio.c'),'-o',str(exe)],check=True)
checks=0
with tempfile.TemporaryDirectory() as temp:
 temp=Path(temp)
 def make(name,layout='mbr',ss=512,bad=None):
  total=70000;fat=(total*4+ss-1)//ss;offset=0 if layout=='raw' else 2048;blocks=total+offset+(100 if layout=='gpt' else 0);path=temp/(name+'.img')
  with path.open('wb') as f:f.truncate(blocks*ss)
  with path.open('r+b') as f:
   def put(lba,data):f.seek(lba*ss);f.write(data)
   boot=bytearray(ss);boot[:3]=b'\xeb\x58\x90';boot[3:11]=b'FRESLEST';struct.pack_into('<HBHBHHBHHHII',boot,11,ss,1,32,2,0,0,0xf8,0,32,64,offset,total)
   struct.pack_into('<IHHIHH',boot,36,fat,0,0,2,1,6);boot[64]=0x80;boot[66]=0x29;struct.pack_into('<I',boot,67,123);boot[71:82]=b'FRESLEST   ';boot[82:90]=b'FAT32   ';boot[510:512]=b'\x55\xaa';put(offset,boot);put(offset+6,boot)
   info=bytearray(ss);struct.pack_into('<I',info,0,0x41615252);struct.pack_into('<III',info,484,0x61417272,0xffffffff,0xffffffff);info[510:512]=b'\x55\xaa';put(offset+1,info)
   table=bytearray(ss);struct.pack_into('<IIII',table,0,0x0ffffff8,0x0fffffff,0x0fffffff,0x0fffffff);put(offset+32,table);put(offset+32+fat,table)
   directory=bytearray(ss);directory[:11]=b'UPDATE  ZIP';directory[11]=0x20;struct.pack_into('<H',directory,26,3);struct.pack_into('<I',directory,28,22);put(offset+32+fat*2,directory);put(offset+32+fat*2+1,b'FRESLEST FAT32 USB TEST')
   if layout!='raw':
    mbr=bytearray(ss);mbr[510:512]=b'\x55\xaa';entry=446
    if layout=='mbr-second':mbr[450]=0x83;struct.pack_into('<II',mbr,454,32,100);entry+=16
    mbr[entry+4]=0xee if layout=='gpt' else 0x0c;struct.pack_into('<II',mbr,entry+8,1 if layout=='gpt' else offset,blocks-1 if layout=='gpt' else total);put(0,mbr)
   if layout=='gpt':
    entries=bytearray(128*128);entries[:16]=bytes.fromhex('a2a0d0ebe5b9334487c068b6b72699c7');entries[16:32]=bytes(range(16));struct.pack_into('<QQ',entries,32,offset,offset+total-1)
    if bad=='outside':struct.pack_into('<Q',entries,40,blocks+999)
    header=bytearray(ss);header[:8]=b'EFI PART';struct.pack_into('<IIIIQQQQ16sQIII',header,8,0x10000,92,0,0,1,blocks-1,34,blocks-34,bytes(range(16)),2,128,128,zlib.crc32(entries))
    if bad=='table_crc':entries[20]^=1
    struct.pack_into('<I',header,16,zlib.crc32(header[:92]))
    if bad=='header_crc':header[60]^=1
    put(1,header);put(2,entries)
   if bad=='boot':boot[510:512]=b'\0\0';put(offset,boot)
  return path
 def check(name,path,ss,ok=True,failures=0):
  global checks
  p=subprocess.run([str(exe),str(path),str(ss),str(int(ok)),str(failures),'4'],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  if p.returncode:raise AssertionError((name,p.returncode,p.stdout,p.stderr))
  print(name+': '+p.stdout.strip());checks+=1
 for ss in (512,4096):
  for layout in ('raw','mbr','mbr-second','gpt'):check(f'{layout}-{ss}',make(f'{layout}-{ss}',layout,ss),ss)
 for bad in ('table_crc','header_crc','outside','boot'):check('gpt-'+bad,make('bad-'+bad,'gpt',bad=bad),512,False)
 check('transient-readiness',make('retry'),512,True,1);check('repeated-read-failure',make('failure'),512,False,50)
print(f'PASS: {checks} real FatFS/MSC cases, four insertion cycles each, zero USB writes')
