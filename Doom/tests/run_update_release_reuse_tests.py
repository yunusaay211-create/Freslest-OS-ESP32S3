"""Ensure the delivered USB collection preserves the current installer's games."""
from pathlib import Path
import argparse,io,sys,zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from repack_doom_installer import read_archive
p=argparse.ArgumentParser();p.add_argument('--base',required=True);p.add_argument('--collection',required=True);a=p.parse_args()
entries={e.name:e for e in read_archive(a.base)[3]}
with zipfile.ZipFile(a.collection) as outer:
    assert set(outer.namelist())=={'UPD88.ZIP','UPD41.ZIP','DOOMVGA.ZIP','OKU-BENI.txt'}
    for profile,name in (('ili9488','UPD88.ZIP'),('ili9341','UPD41.ZIP'),('vga','DOOMVGA.ZIP')):
        with zipfile.ZipFile(io.BytesIO(outer.read(name))) as inner:
            assert inner.testzip() is None
            actual=inner.read('firmware/freslest_doom_s3.bin')
            expected=entries['firmware\\'+profile+'\\freslest_doom_s3.bin'].unpack()
            assert actual==expected,'Release must reuse current installer Doom byte-for-byte: '+profile
print('PASS: all three delivered Doom images match the current installer byte-for-byte')
