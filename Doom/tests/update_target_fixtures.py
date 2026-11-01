"""Valid ESP32-S3 images and ZIP fixtures for the native update host tests."""
import hashlib
import io
import json
import random
import struct
import warnings
import zipfile

BOARD = 'esp32s3-n16r8-ili9341-buttons'
OS_PROJECT = 'freslest_os_s3'
DOOM_PROJECT = 'freslest_doom_s3'
OS_PATH = 'firmware/freslest_os_s3.bin'
DOOM_PATH = 'firmware/freslest_doom_s3.bin'
OS_MARKERS = (
    b'FRESLEST_VIDEO:VGA25175_25175000', b'FRESLEST_LCD:ILI9341',
    b'FRESLEST_UPDATE:OS_AND_DOOM_V3',
)
DOOM_MARKERS = (
    b'FRESLEST_VIDEO:VGA25175_25175000', b'FRESLEST_LCD:ILI9341',
    b'DOOM_VGA:PRIVATE_320x240', b'DOOM_VGA:NATIVE_RGB332_WORDS',
    b'DOOM_BOOT reset=', b'no automatic replay',
)


def esp_image(project=DOOM_PROJECT, version='6.18.1', markers=None, total_size=None):
    """Build one segment with an app descriptor, ESP checksum and appended SHA."""
    if markers is None:
        markers = DOOM_MARKERS if project == DOOM_PROJECT else OS_MARKERS
    length = 8191 if total_size is None else total_size - 65
    assert length >= 512 and len(project.encode('ascii')) < 32 and len(version.encode('ascii')) < 32
    data = bytearray(random.Random(537).randbytes(length))
    data[:256] = bytes(256)
    struct.pack_into('<I', data, 0, 0xabcd5432)
    data[16:16 + len(version) + 1] = version.encode('ascii') + b'\0'
    data[48:48 + len(project) + 1] = project.encode('ascii') + b'\0'
    marker_data = b'\0'.join(markers) + b'\0'
    assert len(marker_data) <= length - 256
    data[256:256 + len(marker_data)] = marker_data
    header = bytearray(24)
    header[:4] = bytes([0xe9, 1, 2, 0x40])
    struct.pack_into('<H', header, 12, 9)
    header[23] = 1
    image = header + struct.pack('<II', 0x3c020020, len(data)) + data
    checksum = 0xef
    for value in data:
        checksum ^= value
    while (len(image) + 1) % 16:
        image.append(0)
    image.append(checksum)
    image.extend(hashlib.sha256(image).digest())
    assert total_size is None or len(image) == total_size
    return bytes(image)


def manifest(target, image):
    assert target in ('os', 'doom')
    value = {
        'format': 'freslest-usb-update',
        'format_version': 3 if target == 'doom' else 2,
        'updater_min': 3 if target == 'doom' else 2,
        'board': BOARD,
        'layout': 'freslest-s3-ab-v2',
        'version': image[48:80].split(b'\0', 1)[0].decode('ascii'),
        'image': {
            'file': DOOM_PATH if target == 'doom' else OS_PATH,
            'size': len(image),
            'sha256': hashlib.sha256(image).hexdigest(),
        },
    }
    if target == 'doom':
        value['target'] = 'doom'
    return value


def encoded_manifest(value):
    return value if isinstance(value, (str, bytes)) else json.dumps(value)


def zip_bytes(entries, compression=zipfile.ZIP_DEFLATED, comment=b''):
    output = io.BytesIO()
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', UserWarning)
        with zipfile.ZipFile(output, 'w', compression=compression) as archive_file:
            for name, content in entries:
                archive_file.writestr(name, content)
            archive_file.comment = comment
    return output.getvalue()


def archive(os_image=None, doom_image=None, os_manifest=None, doom_manifest=None,
            prefix='', extra=(), compression=zipfile.ZIP_DEFLATED, comment=b''):
    entries = []
    if os_image is not None:
        value = manifest('os', os_image) if os_manifest is None else os_manifest
        entries.extend([('freslest-update.json', encoded_manifest(value)), (OS_PATH, os_image)])
    if doom_image is not None:
        value = manifest('doom', doom_image) if doom_manifest is None else doom_manifest
        entries.extend([('freslest-doom-update.json', encoded_manifest(value)), (DOOM_PATH, doom_image)])
    return zip_bytes([(prefix + name, data) for name, data in [*entries, *extra]], compression, comment)
