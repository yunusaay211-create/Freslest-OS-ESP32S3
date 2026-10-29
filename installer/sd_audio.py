"""Install the verified DFPlayer /MP3 pack without formatting a volume."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

NAMES=tuple(f'MP3/{n:04}.mp3' for n in range(1,113))

def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()

def validated_audio(asset_root):
    root=Path(asset_root)
    hashes=json.loads((root/'SHA256.json').read_text(encoding='utf-8'))
    if not isinstance(hashes,dict) or set(hashes)!=set(NAMES):
        raise ValueError('The sound pack must contain MP3/0001.mp3 through MP3/0112.mp3.')
    for name in NAMES:
        if digest(root/name)!=hashes[name]:
            raise ValueError('Invalid sound file: '+name)
    return hashes

def validate_card_volume(path):
    if not str(path).strip():raise ValueError('Select the SD card drive.')
    card=Path(path).absolute()
    if not card.is_dir():raise ValueError('SD card drive not found.')
    if os.name=='nt':
        if str(card)!=card.anchor or str(card).startswith('\\\\'):
            raise ValueError('Select the root of the SD card drive, for example E:\\.')
        if card.drive.upper()==os.environ.get('SystemDrive','C:').upper():
            raise ValueError('The Windows system drive cannot be used as an SD card.')
        fs=ctypes.create_unicode_buffer(32)
        if not ctypes.windll.kernel32.GetVolumeInformationW(str(card),None,0,None,None,None,fs,len(fs)):
            raise OSError('Cannot read the card filesystem.')
        if fs.value.upper() not in ('FAT','FAT32'):
            raise ValueError('DFPlayer requires a FAT/FAT32 card. This installer does not format cards.')
    return card

def install_audio(asset_root,card_root,progress=None):
    assets=Path(asset_root)
    hashes=validated_audio(assets) # Verify the entire source before any writes.
    card=validate_card_volume(card_root)
    folder=card/'MP3'
    if folder.is_symlink() or (folder.exists() and not folder.is_dir()):
        raise ValueError('MP3 must be a regular folder on the selected card.')
    changes=[]
    growth=required=0
    for name in NAMES:
        target=card/name
        if target.is_symlink() or (target.exists() and not target.is_file()):
            raise ValueError('Invalid SD target: '+name)
        if target.is_file() and digest(target)==hashes[name]:continue
        size=(assets/name).stat().st_size
        old_size=target.stat().st_size if target.is_file() else 0
        required=max(required,growth+size)
        growth+=size-old_size
        changes.append(name)
    if shutil.disk_usage(card)[2]<required:
        raise OSError('Not enough free space on the SD card.')
    folder.mkdir(exist_ok=True)
    changed=set(changes)
    copied=0
    for number,name in enumerate(NAMES,1):
        if name in changed:
            temporary=None
            try:
                handle,temporary=tempfile.mkstemp(prefix='.freslest-',suffix='.tmp',dir=folder)
                os.close(handle)
                shutil.copyfile(assets/name,temporary)
                with open(temporary,'r+b') as stream:os.fsync(stream.fileno())
                if digest(temporary)!=hashes[name]:raise ValueError('SD verification failed: '+name)
                os.replace(temporary,card/name)
                temporary=None
                copied+=1
            finally:
                if temporary is not None:
                    try:Path(temporary).unlink(missing_ok=True)
                    except OSError:pass # Removed cards cannot be cleaned until reinserted.
        if progress:progress(number,len(NAMES),name)
    return {'copied':copied,'verified':len(NAMES)}
