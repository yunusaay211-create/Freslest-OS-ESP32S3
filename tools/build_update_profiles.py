"""Rebuild only OS updater profiles; assemble games from the current installer separately.

Use assemble_update_collection.py with the current installer after downloading
these OS images. No Doom, Windows GUI, audio, WAD or filesystem build is invoked.
"""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys
from make_doom_update_zip import checked_image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'installer'))
from release_version import OS_VERSION

def main():
    idf=Path(os.environ['IDF_PATH'])/'tools/idf.py'
    build=ROOT/'OS/build-update-shared';output=ROOT/'native_update/payload'
    build.mkdir(parents=True,exist_ok=True);output.mkdir(parents=True,exist_ok=True)
    if not (build/'sdkconfig').exists():shutil.copyfile(ROOT/'OS/sdkconfig',build/'sdkconfig')
    metadata={'source_commit':os.environ['GITHUB_SHA'],'idf_version':'5.4.4',
              'rebuilt_applications':['freslest_os_s3'],'doom_rebuilt':False,
              'vga_signal':[640,480],'target_clock_hz':25175000,
              'os_canvas':[480,320],'desktop_canvas':[640,480],'physical_hardware_test':False,'profiles':{}}
    for profile,model,preset in (('ili9488',9488,'lcd'),('ili9341',9341,'lcd'),('vga',9341,'vga')):
        subprocess.run([sys.executable,str(idf),'-B',str(build),
            f'-DSDKCONFIG={build/"sdkconfig"}',f'-DFRESLEST_LCD_MODEL={model}',
            f'-DFRESLEST_VGA_PROFILE={preset}','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','build'],
            cwd=ROOT/'OS',env={**os.environ,'IDF_COMPONENT_MANAGER':'0','IDF_TARGET':'esp32s3'},check=True)
        image=checked_image(build/'freslest_os_s3.bin','freslest_os_s3',0x1b0000,profile=profile)
        if b'FRESLEST_UPDATE:SELECT_OS_AND_DOOM' not in image:
            raise ValueError('OS lacks independent OS/Doom selection')
        if image[48:80].split(b'\0',1)[0]!=OS_VERSION.encode('ascii'):raise ValueError(f'OS version differs from release {OS_VERSION}')
        stage=output/profile;stage.mkdir(exist_ok=True)
        (stage/'freslest_os_s3.bin').write_bytes(image)
        metadata['profiles'][profile]={'lcd_model':model,'default_output':preset,
            'os_bytes':len(image),'os_remaining':0x1b0000-len(image),
            'os_sha256':hashlib.sha256(image).hexdigest()}
    (output/'release.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(json.dumps(metadata,indent=2))
if __name__=='__main__':main()
