import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'installer'))
import sd_audio

class SdAudioTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root=Path(self.temp.name)
        self.assets=root/'assets';self.card=root/'card'
        (self.assets/'MP3').mkdir(parents=True);self.card.mkdir()
        self.real_validate=sd_audio.validate_card_volume
        # File-copy tests use temp folders on every OS; volume selection is a
        # separate Windows GUI boundary, not a requirement of this fixture.
        self.volume=patch('sd_audio.validate_card_volume',side_effect=lambda path:Path(path))
        self.volume.start();self.addCleanup(self.volume.stop)
        hashes={}
        for name in sd_audio.NAMES:
            p=self.assets/name;p.write_bytes(b'Freslest audio '+name.encode())
            hashes[name]=hashlib.sha256(p.read_bytes()).hexdigest()
        (self.assets/'SHA256.json').write_text(json.dumps(hashes))

    def test_empty_or_missing_card_rejected(self):
        for path in ('',self.card/'missing'):
            with self.assertRaises(ValueError):self.real_validate(path)

    @unittest.skipUnless(sys.platform=='win32','Windows volume boundary')
    def test_windows_rejects_subfolders_and_system_drive(self):
        with self.assertRaises(ValueError):self.real_validate(self.card)
        system=Path(os.environ.get('SystemDrive','C:')+'\\')
        with self.assertRaises(ValueError):self.real_validate(system)

    def test_copies_112_and_preserves_other_files(self):
        (self.card/'keep.txt').write_text('keep')
        progress=[]
        result=sd_audio.install_audio(self.assets,self.card,lambda *args:progress.append(args))
        self.assertEqual(result,{'copied':112,'verified':112})
        self.assertEqual((self.card/'keep.txt').read_text(),'keep')
        self.assertEqual(len(progress),112)
        self.assertEqual(sd_audio.install_audio(self.assets,self.card),{'copied':0,'verified':112})

    def test_bad_source_does_not_write(self):
        (self.assets/sd_audio.NAMES[-1]).write_bytes(b'bad')
        with self.assertRaises(ValueError):sd_audio.install_audio(self.assets,self.card)
        self.assertEqual(list(self.card.iterdir()),[])

    def test_missing_sound_does_not_write(self):
        (self.assets/sd_audio.NAMES[0]).unlink()
        with self.assertRaises((ValueError,OSError)):sd_audio.install_audio(self.assets,self.card)
        self.assertEqual(list(self.card.iterdir()),[])

    def test_mp3_symlink_rejected(self):
        (self.card/'MP3').symlink_to(self.assets/'MP3',target_is_directory=True)
        with self.assertRaises(ValueError):sd_audio.install_audio(self.assets,self.card)

    def test_target_symlink_rejected(self):
        (self.card/'MP3').mkdir()
        (self.card/sd_audio.NAMES[-1]).symlink_to(self.assets/sd_audio.NAMES[-1])
        with self.assertRaises(ValueError):sd_audio.install_audio(self.assets,self.card)
        self.assertEqual(len(list((self.card/'MP3').iterdir())),1)

    def test_full_card_does_not_overwrite(self):
        (self.card/'MP3').mkdir();target=self.card/sd_audio.NAMES[0];target.write_bytes(b'old')
        with patch('sd_audio.shutil.disk_usage',return_value=(100,100,0)):
            with self.assertRaises(OSError):sd_audio.install_audio(self.assets,self.card)
        self.assertEqual(target.read_bytes(),b'old')

    def test_replacements_only_need_peak_temporary_space(self):
        (self.card/'MP3').mkdir()
        largest=0
        for name in sd_audio.NAMES:
            data=(self.assets/name).read_bytes();(self.card/name).write_bytes(b'x'*len(data));largest=max(largest,len(data))
        with patch('sd_audio.shutil.disk_usage',return_value=(10000,10000-largest,largest)):
            self.assertEqual(sd_audio.install_audio(self.assets,self.card)['copied'],112)

    def test_interrupted_copy_or_flush_keeps_old_file(self):
        (self.card/'MP3').mkdir();target=self.card/sd_audio.NAMES[0]
        for operation in ('shutil.copyfile','os.fsync','os.replace'):
            with self.subTest(operation=operation):
                target.write_bytes(b'old')
                with patch('sd_audio.'+operation,side_effect=OSError('device disconnected')):
                    with self.assertRaises(OSError):sd_audio.install_audio(self.assets,self.card)
                self.assertEqual(target.read_bytes(),b'old')
                self.assertEqual(list((self.card/'MP3').iterdir()),[target])

    def test_corrupt_copy_keeps_old_file(self):
        (self.card/'MP3').mkdir();target=self.card/sd_audio.NAMES[0];target.write_bytes(b'old')
        with patch('sd_audio.shutil.copyfile',side_effect=lambda source,dest:Path(dest).write_bytes(b'corrupted')):
            with self.assertRaises(ValueError):sd_audio.install_audio(self.assets,self.card)
        self.assertEqual(target.read_bytes(),b'old')
        self.assertEqual(list((self.card/'MP3').iterdir()),[target])

if __name__=='__main__':unittest.main()
