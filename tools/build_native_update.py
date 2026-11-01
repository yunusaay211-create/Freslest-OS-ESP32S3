"""Build the VGA OS/Doom pair and verify its two-stage native USB package.

Run in the official ESP-IDF v5.4.4 environment. Other display profiles and
the installer runtime are deliberately reused from the verified release.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

from make_doom_update_zip import checked_image, make_package

ROOT = Path(__file__).resolve().parents[1]


def run(command, cwd=ROOT):
    subprocess.run(command, cwd=cwd, check=True,
                   env={**os.environ, 'IDF_COMPONENT_MANAGER': '0', 'IDF_TARGET': 'esp32s3'})


def main():
    idf = Path(os.environ.get('IDF_PATH', '')) / 'tools/idf.py'
    if not idf.is_file():
        raise SystemExit('Use an ESP-IDF v5.4.4 environment.')
    stage = ROOT / 'native_update/payload'
    stage.mkdir(parents=True, exist_ok=True)
    output = ROOT / 'OS/build-vga-native'
    run([sys.executable, str(idf), '-B', str(output),
         '-DFRESLEST_LCD_MODEL=9341', '-DFRESLEST_VGA_PROFILE=vga',
         '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', 'build'], ROOT / 'OS')
    os_path = output / 'freslest_os_s3.bin'
    os_binary = checked_image(os_path, 'freslest_os_s3', 0x1B0000)
    if os_binary[48:80].split(b'\0', 1)[0] != b'1.1':
        raise ValueError('OS version differs from installer release 1.1')
    run([sys.executable, str(ROOT / 'doom_updater/build_firmware.py')])
    doom_path = ROOT / 'doom_updater/payload/freslest_doom_s3.bin'
    doom_binary = checked_image(doom_path, 'freslest_doom_s3', 0xE0000)
    os_staged = stage / os_path.name
    doom_staged = stage / doom_path.name
    os_staged.write_bytes(os_binary)
    doom_staged.write_bytes(doom_binary)
    shutil.copyfile(ROOT / 'doom_updater/payload/manifest.json', stage / 'doom-manifest.json')
    bundle = stage / 'Freslest-Update-1.1-Doom-Audio.zip'
    make_package(os_staged, doom_staged, bundle)
    # Exercise unmodified old updater C and new updater C against the exact
    # cross-built archive, not only synthetic images.
    run([sys.executable, str(ROOT / 'Doom/tests/run_update_target_tests.py'),
         '--bundle', str(bundle)])
    commit = os.environ.get('GITHUB_SHA') or subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    metadata = {
        'source_commit': commit,
        'idf_version': '5.4.4',
        'board': 'esp32s3-n16r8-ili9341-buttons',
        'layout': 'freslest-s3-ab-v2',
        'native_doom_format': 3,
        'bootstrap_os_format': 2,
        'old_os_selections': 2,
        'vga_signal': [640, 480],
        'target_clock_hz': 25175000,
        'vga_totals': [800, 525],
        'os_canvas': [480, 320],
        'audio_status_replay': False,
        'audio_first_online_confirmation_replay': False,
        'physical_hardware_test': False,
        'files': {},
    }
    for path, limit in ((os_staged, 0x1B0000), (doom_staged, 0xE0000), (bundle, None)):
        data = path.read_bytes()
        info = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        if limit is not None:
            info.update(partition_bytes=limit, remaining_bytes=limit - len(data))
        metadata['files'][path.name] = info
    (stage / 'release.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
    main()
