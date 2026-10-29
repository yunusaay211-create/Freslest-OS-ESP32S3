"""Regression for Windows one-file extraction before Python can start."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from check_windows_archive import check_archive

class WindowsArchivePaths(unittest.TestCase):
    def package(self,folder,name):
        encoded=name.encode()+b'\0'
        table=struct.pack('!iIIIBc',18+len(encoded),0,1,1,0,b'x')+encoded
        cookie=struct.pack('!8sIIII64s',b'MEI\x0c\x0b\x0a\x0b\x0e',1+len(table)+88,1,len(table),311,b'python311.dll')
        exe=Path(folder)/'test.exe';exe.write_bytes(b'MZ'+bytes(62)+b'x'+table+cookie)
        return exe
    def test_rejects_reported_audio_path(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(ValueError,'audio_pack/MP3/0001.mp3'):
                check_archive(self.package(folder,'audio_pack/MP3/0001.mp3'))
    def test_accepts_nested_windows_audio_path(self):
        with tempfile.TemporaryDirectory() as folder:
            paths=check_archive(self.package(folder,'audio_pack\\MP3\\0001.mp3'))
            self.assertEqual(paths,['audio_pack\\MP3\\0001.mp3'])
    def test_rejects_unsafe_target(self):
        with tempfile.TemporaryDirectory() as folder:
            for name in ('..\\outside.mp3','C:\\outside.mp3','\\absolute.mp3'):
                with self.subTest(name=name),self.assertRaises(ValueError):
                    check_archive(self.package(folder,name))
    def test_required_pack_must_have_all_112_sounds(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(ValueError,'112'):
                check_archive(self.package(folder,'audio_pack\\MP3\\0001.mp3'),require_sd_audio=True)

if __name__=='__main__':unittest.main()
