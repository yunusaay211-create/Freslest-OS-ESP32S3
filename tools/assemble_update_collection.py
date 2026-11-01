"""Assemble delivery ZIPs with the exact Doom images in the current installer."""
from pathlib import Path
import argparse,hashlib,json,shutil,tempfile,zipfile
from check_windows_archive import check_archive
from repack_doom_installer import read_archive
from make_doom_update_zip import checked_image,make_package,make_doom_package

def sha(data):return hashlib.sha256(data).hexdigest()

def assemble(base,profiles,output,packages_dir,source_commit):
    base,profiles,output,packages_dir=map(Path,(base,profiles,output,packages_dir))
    check_archive(base,require_sd_audio=True)
    entries={e.name:e for e in read_archive(base)[3]}
    games={};os_images={}
    for profile in ('ili9488','ili9341','vga'):
        prefix='firmware\\'+profile+'\\'
        data=entries[prefix+'freslest_doom_s3.bin'].unpack()
        hashes=json.loads(entries[prefix+'SHA256.json'].unpack())
        if sha(data)!=hashes['firmware/freslest_doom_s3.bin']:
            raise ValueError('Installer Doom checksum mismatch: '+profile)
        games[profile]=data
        os_images[profile]=checked_image(profiles/profile/'freslest_os_s3.bin','freslest_os_s3',0x1b0000,profile=profile)
        if b'FRESLEST_UPDATE:SELECT_OS_AND_DOOM' not in os_images[profile]:
            raise ValueError('OS lacks independent OS/Doom selection')
    output.parent.mkdir(parents=True,exist_ok=True)
    packages_dir.mkdir(parents=True,exist_ok=True)
    report={'firmware_source_commit':source_commit,'base_installer_sha256':sha(base.read_bytes()),
            'doom_sha256':{p:sha(b) for p,b in games.items()},'packages':{}}
    with tempfile.TemporaryDirectory(dir=output.parent) as tmp:
        temp=Path(tmp)
        for profile in games:
            stage=temp/profile;stage.mkdir()
            (stage/'freslest_os_s3.bin').write_bytes(os_images[profile])
            (stage/'freslest_doom_s3.bin').write_bytes(games[profile])
            checked_image(stage/'freslest_doom_s3.bin','freslest_doom_s3',0xe0000,profile=profile,require_native_doom=profile=='vga')
        for profile,name in (('ili9488','UPD88.ZIP'),('ili9341','UPD41.ZIP')):
            stage=temp/profile
            make_package(stage/'freslest_os_s3.bin',stage/'freslest_doom_s3.bin',temp/name,profile=profile,reuse_doom=True)
        make_doom_package(temp/'vga/freslest_doom_s3.bin',temp/'DOOMVGA.ZIP',profile='vga',reuse_doom=True)
        names=('UPD88.ZIP','UPD41.ZIP','DOOMVGA.ZIP')
        instructions=('Freslest OS + Doom Update\n\nTURKCE\nBu DIS ZIP dosyasini acin.\n'
            'UPD88.ZIP, UPD41.ZIP ve DOOMVGA.ZIP dosyalarini ACMADAN USB bellegin kokune kopyalayin.\n'
            'Eski OS ekranina uygun OS paketini listeler. Onu uygulayin.\n'
            'OS yeniden acildiktan sonra en az 10 saniye bekleyin. Freslest Update artik\n'
            'OS ve Doom seceneklerini ayri listeler. Panel tipi ve kayitli LCD/VGA cikisi korunur.\n\n'
            'ENGLISH\nExtract this OUTER ZIP. Copy UPD88.ZIP, UPD41.ZIP and DOOMVGA.ZIP\n'
            'UNEXTRACTED to the USB drive root. Apply the matching OS package. After\n'
            'OS reboots, wait at least 10 seconds. Freslest Update then lists OS and Doom\n'
            'separately for the current display. Panel type and saved output are preserved.\n')
        archive=temp/'collection.zip'
        with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for name in names:z.write(temp/name,name)
            z.writestr('OKU-BENI.txt',instructions)
        with zipfile.ZipFile(archive) as z:
            if z.testzip() is not None:raise ValueError('Collection CRC failure')
        for name in names:
            raw=(temp/name).read_bytes();report['packages'][name]={'bytes':len(raw),'sha256':sha(raw)}
            shutil.copyfile(temp/name,packages_dir/name)
        report['collection']={'bytes':archive.stat().st_size,'sha256':sha(archive.read_bytes())}
        archive.replace(output)
    return report

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('base','profiles','output','packages-dir','source-commit'):p.add_argument('--'+name,required=True)
    a=p.parse_args();print(json.dumps(assemble(a.base,a.profiles,a.output,a.packages_dir,a.source_commit),indent=2))
