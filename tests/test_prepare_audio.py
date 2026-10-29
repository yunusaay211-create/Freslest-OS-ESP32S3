import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import prepare_audio

class PrepareAudioTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);(self.root/'audio-payload').mkdir()
        payload=io.BytesIO();hashes={}
        with zipfile.ZipFile(payload,'w') as archive:
            for name in prepare_audio.NAMES:
                data=b'audio '+name.encode();archive.writestr(name,data);hashes[name]=hashlib.sha256(data).hexdigest()
            archive.writestr('SHA256.json',json.dumps(hashes))
        self.data=payload.getvalue();self.part=self.root/'audio-payload/000.part';self.part.write_bytes(self.data)
        self.manifest=self.root/'audio-payload/SHA256.json'
        self.manifest.write_text(json.dumps({'000.part':hashlib.sha256(self.data).hexdigest()}))
        self.root_patch=patch.object(prepare_audio,'ROOT',self.root);self.root_patch.start();self.addCleanup(self.root_patch.stop)
    def test_fresh_checkout_restores_complete_pack(self):
        target=prepare_audio.prepare()
        self.assertEqual(len(list((target/'MP3').glob('*.mp3'))),112)
        self.assertEqual(prepare_audio.prepare(),target)
    def test_corrupt_part_rejected_before_extract(self):
        self.part.write_bytes(b'corrupted')
        with self.assertRaises(ValueError):prepare_audio.prepare()
        self.assertFalse((self.root/'audio_pack').exists())
    def test_entry_outside_sound_list_rejected_before_extract(self):
        payload=io.BytesIO(self.data)
        with zipfile.ZipFile(payload,'a') as archive:archive.writestr('../outside.txt',b'bad')
        data=payload.getvalue();self.part.write_bytes(data)
        self.manifest.write_text(json.dumps({'000.part':hashlib.sha256(data).hexdigest()}))
        with self.assertRaises(ValueError):prepare_audio.prepare()
        self.assertFalse((self.root/'outside.txt').exists())
        self.assertFalse((self.root/'audio_pack').exists())
    def test_existing_invalid_pack_is_not_silently_packaged(self):
        target=prepare_audio.prepare();(target/'MP3/0112.mp3').write_bytes(b'corrupted')
        with self.assertRaises(ValueError):prepare_audio.prepare()

if __name__=='__main__':unittest.main()
