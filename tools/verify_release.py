"""Validate the packaged images/manifests. Optional --baseline checks retained files."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import sys
import zipfile

root=Path(__file__).resolve().parents[1]
def image(name,version,limit):
    b=(root/'firmware'/name).read_bytes()
    assert 288 <= len(b) <= limit and b[0] == 0xe9
    assert struct.unpack_from('<H',b,12)[0] == 9 and b[3]>>4 == 4 and b[23] == 1
    assert struct.unpack_from('<I',b,32)[0] == 0xabcd5432
    assert b[48:80].split(b'\0',1)[0].decode() == version
    assert b[80:112].split(b'\0',1)[0].decode() == name.removesuffix('.bin')
    offset,checksum=24,0xef
    for _ in range(b[1]):
        size=struct.unpack_from('<I',b,offset+4)[0];offset+=8
        assert offset+size<=len(b)
        for v in b[offset:offset+size]:checksum^=v
        offset+=size
    aligned=(offset+16)&~15
    assert aligned+32==len(b) and b[aligned-1]==checksum and hashlib.sha256(b[:aligned]).digest()==b[aligned:]
    return {'size':len(b),'spare':limit-len(b),'sha256':hashlib.sha256(b).hexdigest(),'version':version}

def verify(baseline=None):
    apps={'OS':image('freslest_os_s3.bin','1.0',0x1b0000),'Doom':image('freslest_doom_s3.bin','6.10',0xe0000)}
    for name,expected in json.loads((root/'firmware/SHA256.json').read_text()).items():
        assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected,name
    with zipfile.ZipFile(root/'updates/Freslest-Update-1.0.zip') as z:
        assert z.testzip() is None
        m=json.loads(z.read('freslest-update.json'))
        assert m['format_version']==2 and m['layout']=='freslest-s3-ab-v2' and m['version']=='1.0'
        assert z.read(m['image']['file'])==(root/m['image']['file']).read_bytes()
        assert m['image']['sha256']==apps['OS']['sha256'] and m['image']['size']==apps['OS']['size']
    for args in [[],['--os-only'],['--doom-only'],['--files-only'],['--with-files']]:
        subprocess.run([sys.executable,str(root/'tools/flash_freslest.py'),'COM5',*args,'--dry-run'],check=True,stdout=subprocess.DEVNULL)
    retained=0
    if baseline:
        with zipfile.ZipFile(baseline) as z:
            for name in z.namelist():
                if name.endswith('/'):continue
                rel=Path(name).relative_to(root.name)
                keep=rel.parts[0] in {'Doom','MP3','ADVERT','common','shared_components'} or str(rel) in {'partitions.csv','OS/partitions.csv','OS/freslest/settings.h','OS/freslest/settings.cpp','firmware/freslest_doom_s3.bin','firmware/doom_flash.wad','firmware/littlefs.bin','firmware/partition-table.bin','firmware/ota_data_initial.bin'} or str(rel).startswith('OS/freslest/data/')
                if keep:
                    assert (root/rel).read_bytes()==z.read(name),str(rel)
                    retained+=1
    print('PASS: OS 1.0 and retained Doom 6.10 descriptors/chip/flash/limits/XOR/SHA; all manifest entries; update v2 bytes; five flasher maps; retained baseline files:',retained)
    print(json.dumps(apps,indent=2))
    return apps,retained

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--baseline',type=Path)
    verify(parser.parse_args().baseline)
