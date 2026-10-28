"""Only the existing Doom partition can be written by this updater."""
from pathlib import Path
import hashlib,json,struct,tempfile,sys
ADDRESS=0x20000
LIMIT=0xE0000
ROOT=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parent))
def image_path(directory=None):
 directory=Path(directory) if directory is not None else ROOT/'payload'
 p=directory/'freslest_doom_s3.bin';b=p.read_bytes();m=json.loads((directory/'manifest.json').read_text())
 if not 288<=len(b)<=LIMIT or b[0]!=0xe9 or struct.unpack_from('<H',b,12)[0]!=9 or b[80:112].split(b'\0')[0]!=b'freslest_doom_s3' or hashlib.sha256(b).hexdigest()!=m['sha256']:
  raise ValueError('Doom dosyası geçersiz.')
 if b'FRESLEST_VIDEO:VGA25175_25175000' not in b or b'DOOM_VGA:PRIVATE_320x240' not in b:
  raise ValueError('Doom VGA sürümü eşleşmiyor.')
 return p

def check_partition_table(data):
 entries={}
 for pos in range(0,min(len(data),0xC00),32):
  magic,kind,sub,offset,size,label,flags=struct.unpack_from('<HBBII16sI',data,pos)
  if magic!=0x50aa:break
  entries[label.split(b'\0')[0].decode('ascii')]=(kind,sub,offset,size)
 wanted={'doom':(0,0x12,ADDRESS,LIMIT),'os_a':(0,0x10,0x100000,0x1b0000),'os_b':(0,0x11,0x2b0000,0x1b0000)}
 if any(entries.get(k)!=v for k,v in wanted.items()):
  raise ValueError('Cihazın bölüm düzeni bu Doom güncellemesine uygun değil. Hiçbir bölüm yazılmadı.')

def flash_doom(port,baud=460800,run_cli=None,directory=None):
 p=image_path(directory)
 if run_cli is None:
  import esptool
  run_cli=esptool.main
 base=['--chip','esp32s3','--port',port,'--baud',str(baud),'--before','default_reset']
 with tempfile.TemporaryDirectory() as d:
  table=Path(d)/'partitions.bin'
  run_cli(base+['--after','no_reset','read_flash','0x8000','0x1000',str(table)])
  check_partition_table(table.read_bytes())
  # No erase-all, OS, NVS, bootloader, partition table, WAD, saves or OTA-data write.
  run_cli(base+['--after','hard_reset','write_flash','-z',hex(ADDRESS),str(p)])
