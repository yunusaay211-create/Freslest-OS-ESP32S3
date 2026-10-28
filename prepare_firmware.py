"""Extract the preserved payload, stamp OS 1.1 and verify all seven files."""
import hashlib
import json
from pathlib import Path
import zipfile
from firmware_release import stamp_release

ROOT = Path(__file__).resolve().parent


def prepare():
    firmware = ROOT / 'firmware'
    if not firmware.exists():
        payload = ROOT / 'firmware-payload.zip'
        if not payload.exists():
            parts = sorted((ROOT / 'firmware-payload').glob('*.part'))
            if not parts:
                raise FileNotFoundError('Firmware payload is missing.')
            data = b''.join(part.read_bytes() for part in parts)
            if hashlib.sha256(data).hexdigest() != '9ad4cafb5ed7c67dc04e36a9dae515d98b73f4dd51a6fa9bba3492d8da938109':
                raise ValueError('Firmware payload checksum mismatch')
            payload.write_bytes(data)
        if hashlib.sha256(payload.read_bytes()).hexdigest() != '9ad4cafb5ed7c67dc04e36a9dae515d98b73f4dd51a6fa9bba3492d8da938109':
            raise ValueError('Firmware payload checksum mismatch')
        with zipfile.ZipFile(payload) as archive:
            for item in archive.infolist():
                path = Path(item.filename)
                if path.is_absolute() or '..' in path.parts or not path.parts or path.parts[0] != 'firmware':
                    raise ValueError('Invalid firmware payload entry')
            archive.extractall(ROOT)
    stamp_release(firmware)
    manifest = json.loads((firmware / 'SHA256.json').read_text(encoding='utf-8'))
    for name in ('bootloader.bin', 'partition-table.bin', 'ota_data_initial.bin',
                 'freslest_doom_s3.bin', 'freslest_os_s3.bin', 'doom_flash.wad', 'littlefs.bin'):
        if hashlib.sha256((firmware / name).read_bytes()).hexdigest() != manifest['firmware/' + name]:
            raise ValueError('Firmware checksum mismatch: ' + name)
    print('OS 1.1 descriptor stamped; all seven release firmware hashes verified.')


if __name__ == '__main__':
    prepare()
