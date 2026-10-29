"""Restore and verify the bundled DFPlayer pack on fresh build checkouts."""
import hashlib
import io
import json
from pathlib import Path
import sys
import zipfile

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'installer'))
from sd_audio import NAMES,validated_audio

def prepare():
    target=ROOT/'audio_pack'
    if target.exists():
        validated_audio(target)
        return target
    parts=sorted((ROOT/'audio-payload').glob('*.part'))
    if not parts:raise FileNotFoundError('Bundled audio-payload parts are missing.')
    manifest=json.loads((ROOT/'audio-payload/SHA256.json').read_text(encoding='utf-8'))
    if set(manifest)!={part.name for part in parts}:
        raise ValueError('Audio payload part list mismatch.')
    for part in parts:
        if hashlib.sha256(part.read_bytes()).hexdigest()!=manifest[part.name]:
            raise ValueError('Audio payload checksum mismatch: '+part.name)
    data=b''.join(part.read_bytes() for part in parts)
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        if len(archive.namelist())!=113 or set(archive.namelist())!=set(NAMES)|{'SHA256.json'}:
            raise ValueError('Audio archive entry list mismatch.')
        # Fixed, verified names also reject path traversal and symlink entries.
        for name in (*NAMES,'SHA256.json'):
            path=target/name
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(archive.read(name))
    validated_audio(target)
    return target

if __name__=='__main__':
    prepare()
    print('Bundled SD sounds ready: 112 verified files.')
