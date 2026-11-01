"""Build OS updater profiles only; retain verified Doom/Windows binaries."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess, sys, zipfile
from make_doom_update_zip import checked_image, make_package, make_doom_package
ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    idf = Path(os.environ['IDF_PATH']) / 'tools/idf.py'
    build = ROOT / 'OS/build-update-shared'
    output = ROOT / 'native_update/payload'
    build.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    if not (build / 'sdkconfig').exists():
        shutil.copyfile(ROOT / 'OS/sdkconfig', build / 'sdkconfig')
    reused = ROOT / 'native_update/reused'
    sources = {'ili9488': reused / 'lcd/ili9488/freslest_doom_s3.bin',
               'ili9341': reused / 'lcd/ili9341/freslest_doom_s3.bin',
               'vga': reused / 'vga/freslest_doom_s3.bin'}
    for profile, path in sources.items():
        checked_image(path, 'freslest_doom_s3', 0xe0000, profile=profile,
                      require_native_doom=profile == 'vga')
        manifest = json.loads((path.parent / ('release.json' if profile == 'vga' else 'SHA256.json')).read_text())
        expected = (manifest['files'][path.name]['sha256'] if profile == 'vga'
                    else manifest['firmware/freslest_doom_s3.bin'])
        if sha(path) != expected:
            raise ValueError('Reused Doom checksum mismatch: ' + profile)
    metadata = {'source_commit': os.environ['GITHUB_SHA'], 'idf_version': '5.4.4',
                'rebuilt_applications': ['freslest_os_s3'],
                'reused_applications': ['freslest_doom_s3'],
                'vga_signal': [640, 480], 'target_clock_hz': 25175000,
                'os_canvas': [480, 320], 'physical_hardware_test': False, 'profiles': {}}
    for profile, model, preset in (('ili9488',9488,'lcd'),('ili9341',9341,'lcd'),('vga',9341,'vga')):
        # Retain SDK objects in one build tree; never invoke the Doom project.
        subprocess.run([sys.executable,str(idf),'-B',str(build),
                        f'-DSDKCONFIG={build / "sdkconfig"}',f'-DFRESLEST_LCD_MODEL={model}',
                        f'-DFRESLEST_VGA_PROFILE={preset}','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','build'],
                       cwd=ROOT/'OS',env={**os.environ,'IDF_COMPONENT_MANAGER':'0','IDF_TARGET':'esp32s3'},check=True)
        image = checked_image(build/'freslest_os_s3.bin','freslest_os_s3',0x1b0000,profile=profile)
        if b'FRESLEST_UPDATE:SELECT_OS_AND_DOOM' not in image:
            raise ValueError('OS lacks independent OS/Doom selection')
        if image[48:80].split(b'\0',1)[0] != b'1.1':
            raise ValueError('OS version differs from installer1.1')
        stage=output/profile; stage.mkdir(exist_ok=True)
        (stage/'freslest_os_s3.bin').write_bytes(image)
        shutil.copyfile(sources[profile],stage/'freslest_doom_s3.bin')
        metadata['profiles'][profile]={'lcd_model':model,'default_output':preset,
            'os_bytes':len(image),'os_remaining':0x1b0000-len(image),
            'os_sha256':sha(stage/'freslest_os_s3.bin'),
            'doom_sha256':sha(stage/'freslest_doom_s3.bin'),'doom_reused':True}
    packages=output/'updates'; packages.mkdir(exist_ok=True)
    # LCD-default bootstrap OS preserves an existing fl_video output selection,
    # including VGA. It never forces a VGA default onto an existing LCD user.
    for profile,model,name in (('ili9488',9488,'UPD88.ZIP'),('ili9341',9341,'UPD41.ZIP')):
        stage=output/profile; bundle=packages/name
        make_package(stage/'freslest_os_s3.bin',stage/'freslest_doom_s3.bin',bundle,
                     profile=profile,reuse_doom=True)
        subprocess.run([sys.executable,str(ROOT/'Doom/tests/run_update_target_tests.py'),
                        '--lcd-model',str(model),'--bundle',str(bundle)],cwd=ROOT,check=True)
    make_doom_package(output/'vga/freslest_doom_s3.bin',packages/'DOOMVGA.ZIP',profile='vga',reuse_doom=True)
    subprocess.run([sys.executable,str(ROOT/'Doom/tests/run_update_output_tests.py'),
                    '--packages',str(packages)],cwd=ROOT,check=True)
    subprocess.run([sys.executable,str(ROOT/'Doom/tests/run_native_doom_runtime_tests.py'),
                    '--packages',str(packages)],cwd=ROOT,check=True)
    instructions=('Freslest OS + Doom Update\n\nTURKCE\nBu DIS ZIP dosyasini acin.\n'
        'UPD88.ZIP, UPD41.ZIP ve DOOMVGA.ZIP dosyalarini ACMADAN USB bellegin kokune kopyalayin.\n'
        'Eski OS ekranina uygun OS paketini listeler. Onu uygulayin.\n'
        'OS yeniden acildiktan sonra en az10saniye bekleyin. Freslest Update artik\n'
        'OS ve Doom seceneklerini ayri listeler. Panel tipi ve kayitli LCD/VGA cikisi korunur.\n\n'
        'ENGLISH\nExtract this OUTER ZIP. Copy UPD88.ZIP, UPD41.ZIP and DOOMVGA.ZIP\n'
        'UNEXTRACTED to the USB drive root. Apply the matching OS package. After\n'
        'OS reboots, wait at least10seconds. Freslest Update then lists OS and Doom\n'
        'separately for the current display. Panel type and saved output are preserved.\n')
    collection=output/'Freslest-Update-1.1-OS-Doom.zip'
    with zipfile.ZipFile(collection,'w',compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(packages.glob('*.ZIP')):archive.write(path,path.name)
        archive.writestr('OKU-BENI.txt',instructions)
    metadata['update_collection']={'name':collection.name,'bytes':collection.stat().st_size,'sha256':sha(collection)}
    metadata['packages']={p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(packages.glob('*.ZIP'))}
    (output/'release.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps(metadata,indent=2))
if __name__=='__main__':main()
