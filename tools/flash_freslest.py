"""Flash the combined OS/Doom layout. No erase-all; saved games/NVS are preserved."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,importlib.util
root=Path(__file__).resolve().parents[1]
def flash_items(with_files=False,files_only=False,doom_only=False,os_only=False):
 if doom_only:return [(0x20000,root/'firmware/freslest_doom_s3.bin',0xe0000)]
 if files_only:return [(0xdb0000,root/'firmware/littlefs.bin',0x50000)]
 result=[(0,root/'firmware/bootloader.bin',0x8000),(0x8000,root/'firmware/partition-table.bin',0x1000),(0x10000,root/'firmware/ota_data_initial.bin',0x2000),(0x100000,root/'firmware/freslest_os_s3.bin',0x1b0000)]
 if not os_only:result.extend([(0x20000,root/'firmware/freslest_doom_s3.bin',0xe0000),(0x460000,root/'firmware/doom_flash.wad',0x950000)])
 if with_files:result.append((0xdb0000,root/'firmware/littlefs.bin',0x50000))
 return result

def main():
 parser=argparse.ArgumentParser(description='Freslest ESP32-S3 N16R8 uploader')
 parser.add_argument('port',help='COM7 or /dev/ttyACM0')
 group=parser.add_mutually_exclusive_group()
 group.add_argument('--with-files',action='store_true',help='Also overwrite LittleFS (ROMs and Note). Use for first installation.')
 group.add_argument('--files-only',action='store_true',help='Overwrite only LittleFS (ROMs and Note).')
 group.add_argument('--doom-only',action='store_true',help='Repair only the Doom firmware in the existing Freslest A/B layout. Keeps OS, WAD, settings, ROMs and saves.')
 group.add_argument('--os-only',action='store_true',help='Repair OS/startup in an existing Freslest A/B layout, booting slot A. Keeps Doom, WAD, NVS, ROMs and saves; resets OTA selection history.')
 parser.add_argument('--dry-run',action='store_true',help='Validate package and print flash map without accessing a board.')
 args=parser.parse_args()
 items=flash_items(args.with_files,args.files_only,args.doom_only,args.os_only)
 manifest=json.loads((root/'firmware/SHA256.json').read_text())
 for address,path,limit in items:
  data=path.read_bytes();relative=path.relative_to(root).as_posix()
  if not data or len(data)>limit:raise ValueError('Partition size violation: '+relative)
  if hashlib.sha256(data).hexdigest()!=manifest[relative]:raise ValueError('Checksum mismatch: '+relative)
  print('0x%06x  %8d bytes  %s'%(address,len(data),relative))
  if address>=0xe00000:raise ValueError('Refusing to overwrite Doom saves')
 if args.files_only or args.with_files:print('LittleFS ROMs/Note will be replaced by the packaged initial image.')
 if args.dry_run:return
 if importlib.util.find_spec('esptool') is None:raise SystemExit('Install first: python -m pip install esptool==4.11.0')
 cmd=[sys.executable,'-m','esptool','--chip','esp32s3','--port',args.port,'--baud','460800','--before','default_reset','--after','hard_reset','write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','16MB']
 for address,path,_ in items:cmd += [hex(address),str(path)]
 subprocess.run(cmd,check=True,cwd=root)
if __name__=='__main__':main()
