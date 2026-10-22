"""Stamp the preserved Freslest ESP32-S3 OS application descriptor as release 1.0.

This is metadata-only release stamping, not an ESP-IDF source rebuild. Only the
32-byte esp_app_desc_t.version field, segment XOR checksum and appended SHA256
digest may change. The original ELF provenance hash, compiled code, other data,
image headers and every other firmware file are retained.

Only the exact, unsigned original image and the exact stamped release image are
accepted. Signed images cannot be restamped without their signing key and are
rejected, as are unexpected bytes after the appended digest. No esptool install
is needed by this script; tests independently validate the output with esptool.

Format references (ESP-IDF 5.4.4, matching the original image):
https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-reference/system/app_image_format.html
https://docs.espressif.com/projects/esptool/en/release-v4/esp32s3/advanced-topics/firmware-image-format.html

Run ``python firmware_release.py [firmware-directory]`` or call stamp_release()
after extracting the preserved payload and before its final manifest check.
"""
import argparse
import functools
import hashlib
import json
import operator
import os
from pathlib import Path
import re
import struct
import tempfile


ORIGINAL_VERSION = '6.16'
RELEASE_VERSION = '1.0'
ORIGINAL_SHA256 = '04a725fba76a2bbbfc302aa3c8283a6b2ee3b1d1c7e15ab89f5519185d092f70'
RELEASE_SHA256 = 'c3815e88e6edce96536e5dedb380e056b80ce9c937f2b4b5468e26f1774455ea'
IMAGE_NAME = 'freslest_os_s3.bin'
MANIFEST_KEY = 'firmware/' + IMAGE_NAME
APP_DESCRIPTOR_OFFSET = 0x20
VERSION_OFFSET = APP_DESCRIPTOR_OFFSET + 16
VERSION_SIZE = 32


def _cstring(data, offset, size, name):
    raw = data[offset:offset + size]
    value, separator, padding = raw.partition(b'\0')
    if not separator or any(padding):
        raise ValueError('Invalid null-terminated app descriptor ' + name)
    try:
        return value.decode('ascii')
    except UnicodeDecodeError as error:
        raise ValueError('Invalid ASCII app descriptor ' + name) from error


def _checksum(data, segments):
    checksum = 0xEF
    for segment in segments:
        start = segment['data_offset']
        checksum = functools.reduce(operator.xor, data[start:start + segment['length']], checksum)
    return checksum


def inspect_image(data):
    """Validate an unsigned S3 app image's structure, descriptor, XOR and SHA256."""
    if len(data) < 24:
        raise ValueError('Truncated ESP image header')
    if data[0] != 0xE9:
        raise ValueError('Invalid ESP image magic')
    if not 1 <= data[1] <= 16:
        raise ValueError('Invalid ESP image segment count')
    chip_id = struct.unpack_from('<H', data, 12)[0]
    if chip_id != 9:
        raise ValueError('Expected an ESP32-S3 image (chip ID 9)')
    if data[23] != 1:
        raise ValueError('Expected an appended SHA256 image digest')

    position = 24
    segments = []
    for index in range(data[1]):
        if position + 8 > len(data):
            raise ValueError('Truncated ESP segment header')
        address, size = struct.unpack_from('<II', data, position)
        start = position + 8
        end = start + size
        if size % 4 or end > len(data) - 33:
            raise ValueError('Invalid or truncated ESP segment data')
        segments.append({'index': index, 'header_offset': position, 'data_offset': start,
                         'load_address': address, 'length': size})
        position = end

    checksum_offset = position | 15  # XOR occupies the last byte of a 16-byte block.
    digest_offset = checksum_offset + 1
    if digest_offset + 32 != len(data):
        raise ValueError('Unexpected image length: truncated digest or trailing signature/data')
    if any(data[position:checksum_offset]):
        raise ValueError('Nonzero ESP image footer padding')
    checksum = _checksum(data, segments)
    if data[checksum_offset] != checksum:
        raise ValueError('ESP segment XOR checksum mismatch')
    digest = hashlib.sha256(data[:digest_offset]).digest()
    if data[digest_offset:] != digest:
        raise ValueError('ESP appended SHA256 digest mismatch')

    first = segments[0]
    if first['data_offset'] != APP_DESCRIPTOR_OFFSET or first['length'] < 256:
        raise ValueError('Missing fixed-offset application descriptor')
    if not 0x3C000000 <= first['load_address'] < 0x3E000000:
        raise ValueError('Application descriptor is not in the ESP32-S3 DROM segment')
    magic, secure_version = struct.unpack_from('<II', data, APP_DESCRIPTOR_OFFSET)
    if magic != 0xABCD5432:
        raise ValueError('Invalid ESP application descriptor magic')
    if secure_version != 0:
        raise ValueError('Unexpected app secure version; release stamping is not authorized')
    project_name = _cstring(data, 0x50, 32, 'project name')
    if project_name != 'freslest_os_s3':
        raise ValueError('Unexpected application project name')

    return {
        'version': _cstring(data, VERSION_OFFSET, VERSION_SIZE, 'version'),
        'project_name': project_name,
        'idf_version': _cstring(data, 0x90, 32, 'ESP-IDF version'),
        'app_elf_sha256': data[0xB0:0xD0].hex(),
        'image_size': len(data),
        'image_sha256': hashlib.sha256(data).hexdigest(),
        'chip_id': chip_id,
        'secure_version': secure_version,
        'entry_point': struct.unpack_from('<I', data, 4)[0],
        'flash_mode': data[2],
        'flash_size_frequency': data[3],
        'segments': segments,
        'checksum_offset': checksum_offset,
        'checksum': checksum,
        'digest_offset': digest_offset,
        'validation_digest': digest.hex(),
    }


def _with_version(data, info, version):
    stamped = bytearray(data)
    stamped[VERSION_OFFSET:VERSION_OFFSET + VERSION_SIZE] = version.encode('ascii').ljust(VERSION_SIZE, b'\0')
    stamped[info['checksum_offset']] = _checksum(stamped, info['segments'])
    digest_offset = info['digest_offset']
    stamped[digest_offset:] = hashlib.sha256(stamped[:digest_offset]).digest()
    return bytes(stamped)


def _outside_metadata_unchanged(before, after, info):
    ranges = ((VERSION_OFFSET, VERSION_OFFSET + VERSION_SIZE),
              (info['checksum_offset'], info['checksum_offset'] + 1),
              (info['digest_offset'], info['digest_offset'] + 32))
    position = 0
    for start, end in ranges:
        if before[position:start] != after[position:start]:
            return False
        position = end
    return len(before) == len(after) and before[position:] == after[position:]


def stamp_image(data):
    """Return the pinned release image, rejecting all unrelated/corrupt inputs."""
    info = inspect_image(data)
    identity = (info['version'], info['image_sha256'])
    if identity == (RELEASE_VERSION, RELEASE_SHA256):
        return bytes(data)
    if identity != (ORIGINAL_VERSION, ORIGINAL_SHA256):
        raise ValueError('Image SHA256 provenance is not a recognized Freslest release input')
    stamped = _with_version(data, info, RELEASE_VERSION)
    result = inspect_image(stamped)
    if result['image_sha256'] != RELEASE_SHA256:
        raise ValueError('Stamped image does not match the pinned release SHA256')
    if not _outside_metadata_unchanged(data, stamped, info):
        raise ValueError('Stamp changed bytes outside the allowed metadata fields')
    return stamped


def _release_report(original, stamped):
    before = inspect_image(original)
    after = inspect_image(stamped)
    if before['image_sha256'] != ORIGINAL_SHA256 or after['image_sha256'] != RELEASE_SHA256:
        raise ValueError('Cannot report unexpected release image identities')
    changed_bytes = [{'offset': offset, 'offset_hex': hex(offset),
                      'before': f'{old:02x}', 'after': f'{new:02x}'}
                     for offset, (old, new) in enumerate(zip(original, stamped)) if old != new]
    segments = []
    for segment in before['segments']:
        start = segment['data_offset']
        end = start + segment['length']
        if segment['index'] == 0:
            old_data = original[start:VERSION_OFFSET] + original[VERSION_OFFSET + VERSION_SIZE:end]
            new_data = stamped[start:VERSION_OFFSET] + stamped[VERSION_OFFSET + VERSION_SIZE:end]
        else:
            old_data, new_data = original[start:end], stamped[start:end]
        if old_data != new_data:
            raise ValueError('Compiled segment bytes changed outside app version')
        segments.append({**segment, 'version_field_excluded': segment['index'] == 0,
                         'unchanged_data_sha256': hashlib.sha256(old_data).hexdigest()})
    return {
        'method': 'application-descriptor release stamping',
        'source_rebuild': False,
        'original_version': ORIGINAL_VERSION,
        'release_version': RELEASE_VERSION,
        'original_sha256': ORIGINAL_SHA256,
        'release_sha256': RELEASE_SHA256,
        'image_size': len(stamped),
        'chip_id': after['chip_id'],
        'secure_version': after['secure_version'],
        'signed_image': False,
        'idf_version': after['idf_version'],
        'preserved_original_elf_sha256': after['app_elf_sha256'],
        'version_offset': VERSION_OFFSET,
        'version_field_size': VERSION_SIZE,
        'checksum_offset': after['checksum_offset'],
        'original_checksum': before['checksum'],
        'release_checksum': after['checksum'],
        'digest_offset': after['digest_offset'],
        'original_validation_digest': before['validation_digest'],
        'release_validation_digest': after['validation_digest'],
        'outside_metadata_unchanged': _outside_metadata_unchanged(original, stamped, before),
        'segments': segments,
        'changed_byte_count': len(changed_bytes),
        'changed_bytes': changed_bytes,
    }


def _atomic_write(path, data):
    if path.exists() and path.read_bytes() == data:
        return
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + '.', suffix='.tmp', delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.chmod(temporary, path.stat().st_mode & 0o777 if path.exists() else 0o644)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def stamp_release(firmware_dir=None):
    """Validate, stamp the OS only, update its one manifest value, and save evidence.

    All validation finishes before the first write. Each replacement is atomic.
    If interrupted after replacing the OS but before replacing the manifest, a
    rerun accepts that exact known pair and completes the manifest/report update.
    """
    directory = Path(firmware_dir) if firmware_dir is not None else Path(__file__).resolve().parent / 'firmware'
    image_path = directory / IMAGE_NAME
    manifest_path = directory / 'SHA256.json'
    original_input = image_path.read_bytes()
    stamped = stamp_image(original_input)
    info = inspect_image(original_input)
    original = original_input if info['version'] == ORIGINAL_VERSION else _with_version(original_input, info, ORIGINAL_VERSION)
    report = _release_report(original, stamped)

    # Preserve the manifest's order, formatting, and every other value verbatim.
    manifest_bytes = manifest_path.read_bytes()
    manifest_text = manifest_bytes.decode('utf-8')
    manifest = json.loads(manifest_text)
    if not isinstance(manifest, dict):
        raise ValueError('Firmware manifest must be a JSON object')
    manifest_hash = manifest.get(MANIFEST_KEY)
    allowed_hashes = {info['image_sha256']}
    if info['image_sha256'] == RELEASE_SHA256:
        allowed_hashes.add(ORIGINAL_SHA256)  # Recover an interrupted atomic OS replacement.
    if not isinstance(manifest_hash, str) or manifest_hash not in allowed_hashes:
        raise ValueError('OS firmware SHA256 manifest does not match the recognized image')
    pattern = r'("firmware/freslest_os_s3\.bin"\s*:\s*")([0-9a-f]{64})(")'
    manifest_text, replacements = re.subn(pattern, lambda match: match[1] + RELEASE_SHA256 + match[3], manifest_text)
    if replacements != 1:
        raise ValueError('Expected exactly one OS firmware manifest entry')
    report_bytes = (json.dumps(report, indent=2) + '\n').encode('utf-8')

    _atomic_write(image_path, stamped)
    _atomic_write(manifest_path, manifest_text.encode('utf-8'))
    _atomic_write(directory / 'VERSION_REPORT.json', report_bytes)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    parser.add_argument('firmware_dir', nargs='?', type=Path, help='defaults to firmware/ next to this script')
    arguments = parser.parse_args()
    try:
        report = stamp_release(arguments.firmware_dir)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Freslest OS {report['release_version']}: descriptor, XOR checksum and SHA256 verified.")
    print(f"OS SHA256: {report['release_sha256']}")


if __name__ == '__main__':
    main()
