"""Build both standalone x64 Windows GUI executables."""
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
from prepare_firmware import prepare
from firmware_release import RELEASE_VERSION

ROOT = Path(__file__).resolve().parent


def verify_display_firmware(firmware_root):
    sys.path.insert(0, str(ROOT / 'installer'))
    from payload import DISPLAY_MODELS, validated_payload
    for model in DISPLAY_MODELS:
        directory = Path(firmware_root) / model
        if not (directory / 'SHA256.json').is_file():
            raise RuntimeError('Build all display profiles first: python tools/build_lcd_profiles.py')
        validated_payload(firmware_root, 'en', model)
        image = (directory / 'freslest_os_s3.bin').read_bytes()
        if image[0x30:0x50].split(b'\0', 1)[0] != RELEASE_VERSION.encode('ascii'):
            raise RuntimeError(f'{model}: bundled OS must report {RELEASE_VERSION}; rebuild the display profiles')


def main():
    if sys.platform != 'win32':
        raise SystemExit('This build must run on Windows with 64-bit Python 3.11.')
    if sys.version_info[:2] != (3, 11) or platform.architecture()[0] != '64bit':
        raise SystemExit('Use 64-bit Python 3.11.')
    os.chdir(ROOT)
    prepare()
    # A dropdown alone is insufficient: package all actual source-built variants.
    verify_display_firmware(ROOT / 'firmware')
    common = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--windowed', '--noupx']
    subprocess.run(common + ['--name', 'Freslest Installer', '--collect-all', 'esptool',
                            '--collect-all', 'serial', '--add-data', 'firmware;firmware',
                            'installer/installer.py'], check=True)
    subprocess.run(common + ['--name', 'Freslest USB Viewer', '--paths', 'viewer',
                            '--collect-all', 'serial', 'viewer/freslest_viewer.py'], check=True)
    hashes = {}
    for name in ('Freslest Installer.exe', 'Freslest USB Viewer.exe'):
        data = (ROOT / 'dist' / name).read_bytes()
        pe = int.from_bytes(data[60:64], 'little')
        if data[:2] != b'MZ' or data[pe:pe + 4] != b'PE\0\0':
            raise RuntimeError('Not a Windows PE executable: ' + name)
        if int.from_bytes(data[pe + 4:pe + 6], 'little') != 0x8664:
            raise RuntimeError('Executable is not x64: ' + name)
        if int.from_bytes(data[pe + 92:pe + 94], 'little') != 2:
            raise RuntimeError('Executable is not a Windows GUI application: ' + name)
        hashes[name] = hashlib.sha256(data).hexdigest()
    (ROOT / 'dist/SHA256SUMS.txt').write_text(''.join(f'{sha}  {name}\n' for name, sha in hashes.items()), encoding='utf-8')
    report = {'release': 'Freslest OS 1.1', 'build_os': platform.platform(),
              'python': sys.version, 'sha256': hashes,
              'hardware_flash_test': False, 'hardware_viewer_test': False}
    (ROOT / 'dist/BUILD_REPORT.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('Both x64 Windows GUI executables built and checked.')


if __name__ == '__main__':
    main()
