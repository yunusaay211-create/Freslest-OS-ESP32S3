"""Stage the local, numbered DFPlayer sounds for the standalone installer."""
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'installer'))
from sd_audio import NAMES,validated_audio

def main():
    sources=[ROOT/name for name in NAMES]
    for path in sources:
        if not path.is_file():raise SystemExit('Missing sound: '+str(path))
    target=ROOT/'audio_pack'
    (target/'MP3').mkdir(parents=True,exist_ok=True)
    hashes={}
    for name in NAMES:
        shutil.copyfile(ROOT/name,target/name)
        with (target/name).open('rb') as stream:hashes[name]=hashlib.file_digest(stream,'sha256').hexdigest()
    (target/'SHA256.json').write_text(json.dumps(hashes,indent=2)+'\n',encoding='utf-8')
    validated_audio(target)
    print('Staged and verified 112 DFPlayer sounds.')

if __name__=='__main__':main()
