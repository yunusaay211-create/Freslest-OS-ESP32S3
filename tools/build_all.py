"""Build both firmware projects in an ESP-IDF v5.4.4 command prompt."""
from pathlib import Path
import os,sys,subprocess,shutil,hashlib,json
root=Path(__file__).resolve().parents[1]
idf=Path(os.environ.get('IDF_PATH',''))
if not (idf/'tools/idf.py').is_file():raise SystemExit('Run this from an ESP-IDF v5.4.4 command prompt.')
env={**os.environ,'IDF_COMPONENT_MANAGER':'0','IDF_TARGET':'esp32s3'}
for name in ['OS','Doom']:
 project=root/name;shutil.copy(root/'partitions.csv',project/'partitions.csv')
 subprocess.run([sys.executable,str(idf/'tools/idf.py'),'-B',str(project/'build'),'build'],cwd=project,env=env,check=True)
filename={'OS':'freslest_os_s3.bin','Doom':'freslest_doom_s3.bin'}
for name,limit in [('OS',0x1b0000),('Doom',0xe0000)]:
 binary=root/name/'build'/filename[name]
 if binary.stat().st_size>limit:raise SystemExit(f'{name} image exceeds its {limit}-byte slot; firmware was not packaged.')
for name,file in filename.items():shutil.copy(root/name/'build'/file,root/'firmware'/file)
for source,dest in [('bootloader/bootloader.bin','bootloader.bin'),('partition_table/partition-table.bin','partition-table.bin'),('ota_data_initial.bin','ota_data_initial.bin')]:shutil.copy(root/'OS/build'/source,root/'firmware'/dest)
subprocess.run([sys.executable,str(root/'tools/make_flash_wad.py')],check=True)
subprocess.run([sys.executable,str(root/'tools/make_update_zip.py')],check=True)
paths=[*sorted((root/'firmware').glob('*.bin')),root/'Doom/DOOM.WAD',root/'firmware/doom_flash.wad']
paths += [root/'freslest-update.json',*sorted((root/'updates').glob('Freslest-Update-*.zip'))]
manifest={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
(root/'firmware/SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Built OS and Doom. Initial LittleFS image is retained; use tools/make_filesystem.py after editing data/.')
