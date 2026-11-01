"""Exercise the dual-manifest update builder with real ESP image fixtures."""
from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import subprocess
import sys
import tempfile
import zipfile

from update_target_fixtures import (
    BOARD, DOOM_MARKERS, DOOM_PATH, DOOM_PROJECT, OS_MARKERS, OS_PATH, OS_PROJECT, esp_image,
)

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tools/make_doom_update_zip.py'


def reseal(image):
    """Repair ESP integrity after changing an independently tested header field."""
    data = bytearray(image)
    offset, checksum = 24, 0xef
    for _ in range(data[1]):
        length = struct.unpack_from('<I', data, offset + 4)[0]
        offset += 8
        for value in data[offset:offset + length]:
            checksum ^= value
        offset += length
    aligned = (offset + 16) & ~15
    data[aligned - 1] = checksum
    data[aligned:] = hashlib.sha256(data[:aligned]).digest()
    return bytes(data)


def main():
    checks = 0
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        os_path, doom_path, output = [temporary / name for name in ('os.bin', 'doom.bin', 'update.zip')]
        os_image, doom_image = esp_image(OS_PROJECT), esp_image(DOOM_PROJECT)
        os_path.write_bytes(os_image)
        doom_path.write_bytes(doom_image)
        result = subprocess.run([sys.executable, str(SCRIPT), '--os', str(os_path), '--doom', str(doom_path),
                                 '--output', str(output)], capture_output=True, text=True)
        assert result.returncode == 0, f'Valid dual bundle CLI failed ({result.returncode}): {result.stdout}{result.stderr}'
        checks += 1
        spec = importlib.util.spec_from_file_location('make_doom_update_zip', SCRIPT)
        builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(builder)
        expected_names = {'freslest-update.json', 'freslest-doom-update.json', OS_PATH, DOOM_PATH, 'README.txt'}
        with zipfile.ZipFile(output) as package:
            assert len(package.infolist()) == 5 and set(package.namelist()) == expected_names
            assert package.testzip() is None
            for target, image, image_path in [('os', os_image, OS_PATH), ('doom', doom_image, DOOM_PATH)]:
                assert package.read(image_path) == image
                name = 'freslest-doom-update.json' if target == 'doom' else 'freslest-update.json'
                value = json.loads(package.read(name))
                expected = {
                    'format': 'freslest-usb-update', 'format_version': 3 if target == 'doom' else 2,
                    'updater_min': 3 if target == 'doom' else 2, 'board': BOARD,
                    'layout': 'freslest-s3-ab-v2', 'version': '6.18.1',
                    'image': {'file': image_path, 'size': len(image), 'sha256': hashlib.sha256(image).hexdigest()},
                }
                if target == 'doom':
                    expected['target'] = 'doom'
                assert value == expected, (target, value)
                checks += 1
        checks += 1
        for project, path, image, limit in [(OS_PROJECT, os_path, os_image, 0x1b0000),
                                            (DOOM_PROJECT, doom_path, doom_image, 0xe0000)]:
            assert builder.checked_image(path, project, limit) == image
            checks += 1

            def reject(name, changed, expected_project=project, capacity=limit):
                nonlocal checks
                invalid = temporary / (name + '.bin')
                invalid.write_bytes(changed)
                try:
                    builder.checked_image(invalid, expected_project, capacity)
                except ValueError:
                    checks += 1
                else:
                    raise AssertionError(project + ': accepted ' + name)

            reject('truncated', image[:280])
            reject('wrong_project', esp_image(DOOM_PROJECT if project == OS_PROJECT else OS_PROJECT))
            reject('invalid_version', esp_image(project, version='bad version'))
            reject('caller_capacity', image, capacity=len(image) - 1)
            reject('unknown_project', image, expected_project='other')
            for name, offset, value in [('wrong_chip', 12, 0), ('wrong_flash', 3, 0x30),
                                        ('no_app_descriptor', 32, 0), ('wrong_magic', 0, 0),
                                        ('missing_appended_digest', 23, 0)]:
                damaged = bytearray(image)
                damaged[offset] = value
                reject(name, reseal(damaged))
            damaged = bytearray(image)
            damaged[1] = 0
            reject('zero_segments', damaged)
            damaged[1] = 17
            reject('too_many_segments', damaged)
            for name, offset in [('unterminated_version', 48), ('unterminated_project', 80)]:
                damaged = bytearray(image)
                damaged[offset:offset + 32] = b'X' * 32
                reject(name, reseal(damaged))
            damaged = bytearray(image)
            damaged[-1] ^= 1
            reject('bad_esp_sha', damaged)
            damaged = bytearray(image)
            damaged[-33] ^= 1
            reject('bad_esp_checksum', damaged)
            markers = OS_MARKERS if project == OS_PROJECT else DOOM_MARKERS
            for index, marker in enumerate(markers):
                reject('missing_marker_' + str(index), esp_image(project, markers=[m for m in markers if m != marker]))
            reject('ili9488_profile', esp_image(project, markers=(*markers, b'FRESLEST_LCD:ILI9488')))
            reject('old_vga_clock', esp_image(project, markers=(*markers, b'FRESLEST_VIDEO:VGA252_25200000')))
            maximum = temporary / (project + '_maximum.bin')
            maximum_image = esp_image(project, total_size=limit)
            maximum.write_bytes(maximum_image)
            assert builder.checked_image(maximum, project, limit) == maximum_image
            checks += 1
            reject('slot_overflow', esp_image(project, total_size=limit + 16))
        destination = temporary / 'nested/update.zip'
        assert Path(builder.make_package(os_path, doom_path, destination)) == destination
        assert destination.is_file()
        checks += 1
        doom_path.write_bytes(esp_image(DOOM_PROJECT, markers=()))
        failed_output = temporary / 'invalid.zip'
        try:
            builder.make_package(os_path, doom_path, failed_output)
        except ValueError:
            assert not failed_output.exists()
            checks += 1
        else:
            raise AssertionError('Invalid Doom image produced a ZIP')
    print(f'PASS: {checks} dual bundle CLI, manifest, slot, ESP integrity and firmware-marker cases')


if __name__ == '__main__':
    main()
