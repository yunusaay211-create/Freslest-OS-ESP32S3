"""Build both LCD variants and the VGA preset from source, then stage verified installer payloads.

Run in an ESP-IDF v5.4.4 environment: python tools/build_lcd_profiles.py
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def build():
    idf = Path(os.environ.get('IDF_PATH', '')) / 'tools/idf.py'
    if not idf.is_file():
        raise SystemExit('Run in an ESP-IDF v5.4.4 environment; all LCD/VGA profiles require a source build.')
    # Restore common WAD/LittleFS assets from the preserved release payload.
    sys.path.insert(0, str(ROOT))
    from prepare_firmware import prepare
    from firmware_release import RELEASE_VERSION
    prepare()
    sys.path.insert(0, str(ROOT / 'installer'))
    from payload import ITEMS, validated_payload, firmware_matches_profile
    for profile,model,video in (('ili9488',9488,'lcd'),('ili9341',9341,'lcd'),('vga',9341,'vga')):
        builds = {}
        for project in ('OS', 'Doom'):
            directory = ROOT / project
            # Stage each completed profile before switching the CMake define.
            # The same build tree reuses unchanged SDK/Arduino dependencies.
            output = directory / 'build-lcd'
            subprocess.run([sys.executable, str(idf), '-B', str(output),
                            f'-DFRESLEST_LCD_MODEL={model}', f'-DFRESLEST_VGA_PROFILE={video}', 'build'], cwd=directory,
                           env={**os.environ, 'IDF_COMPONENT_MANAGER':'0', 'IDF_TARGET':'esp32s3'}, check=True)
            builds[project] = output
        with tempfile.TemporaryDirectory(dir=ROOT / 'firmware') as temp:
            stage = Path(temp)
            sources = {
                'bootloader.bin': builds['OS'] / 'bootloader/bootloader.bin',
                'partition-table.bin': builds['OS'] / 'partition_table/partition-table.bin',
                'ota_data_initial.bin': builds['OS'] / 'ota_data_initial.bin',
                'freslest_os_s3.bin': builds['OS'] / 'freslest_os_s3.bin',
                'freslest_doom_s3.bin': builds['Doom'] / 'freslest_doom_s3.bin',
            }
            manifest = {}
            for _, name, limit in ITEMS:
                source = sources.get(name, ROOT / 'firmware' / name)
                data = source.read_bytes()
                if not data or len(data) > limit:
                    raise ValueError(f'{profile}: {name} does not fit its partition')
                if name in ('freslest_os_s3.bin', 'freslest_doom_s3.bin'):
                    if not firmware_matches_profile(data,profile):
                        raise ValueError(f'{profile}: firmware has the wrong LCD profile: {name}')
                if name == 'freslest_os_s3.bin' and data[0x30:0x50].split(b'\0',1)[0].decode('ascii') != RELEASE_VERSION:
                    raise ValueError(f'{profile}: compiled OS version differs from installer release')
                shutil.copyfile(source, stage / name)
                manifest['firmware/' + name] = hashlib.sha256(data).hexdigest()
            (stage / 'SHA256.json').write_text(json.dumps(manifest, indent=2) + '\n')
            shutil.copytree(stage, ROOT / 'firmware' / profile, dirs_exist_ok=True)
        validated_payload(ROOT / 'firmware', 'en', profile)
        print(f'Built and verified {profile}: OS, Doom and all seven installer segments.')


if __name__ == '__main__':
    build()
