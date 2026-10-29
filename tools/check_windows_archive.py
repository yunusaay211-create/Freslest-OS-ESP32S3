"""Check native Windows extraction paths inside a PyInstaller one-file EXE."""
from pathlib import Path
import struct

COOKIE=struct.Struct('!8sIIII64s')
TOC=struct.Struct('!iIIIBc')
MAGIC=b'MEI\x0c\x0b\x0a\x0b\x0e'

def check_archive(executable,require_sd_audio=False):
    path=Path(executable)
    size=path.stat().st_size
    if size<COOKIE.size:raise ValueError('Windows archive is truncated.')
    with path.open('rb') as stream:
        if stream.read(2)!=b'MZ':raise ValueError('Expected a Windows executable.')
        stream.seek(-COOKIE.size,2)
        magic,package_size,toc_offset,toc_size,_,_=COOKIE.unpack(stream.read(COOKIE.size))
        if magic!=MAGIC or package_size>size or toc_offset+toc_size>package_size-COOKIE.size:
            raise ValueError('Invalid Windows archive cookie.')
        stream.seek(size-package_size+toc_offset)
        table=stream.read(toc_size)
    paths=[];offset=0
    while offset<len(table):
        if len(table)-offset<TOC.size:raise ValueError('Invalid Windows archive table.')
        length,_,_,_,_,kind=TOC.unpack_from(table,offset)
        if length<=TOC.size or offset+length>len(table):raise ValueError('Invalid Windows archive entry.')
        name=table[offset+TOC.size:offset+length].rstrip(b'\0').decode('utf-8')
        offset+=length
        if kind not in (b'b',b'x',b'Z'):continue
        # Windows bootloader parent creation recognizes PYI_SEP ('\\').
        # Forward slashes reach fopen without the nested directories created.
        if '/' in name or ':' in name or any(part in ('','..','.') for part in name.split('\\')):
            raise ValueError('Invalid Windows extraction path: '+name)
        if name in paths:raise ValueError('Duplicate Windows extraction path: '+name)
        paths.append(name)
    if require_sd_audio:
        expected={f'audio_pack\\MP3\\{number:04}.mp3' for number in range(1,113)}|{'audio_pack\\SHA256.json'}
        actual={name for name in paths if name.startswith('audio_pack\\')}
        if actual!=expected:raise ValueError('Installer must contain all 112 SD sounds and their manifest.')
    return paths

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--sd-audio',action='store_true')
    options=parser.parse_args()
    paths=check_archive(options.executable,options.sd_audio)
    print(f'Windows extraction paths verified: {len(paths)} files.')
