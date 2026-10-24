"""Regression checks for the release version in the actual ESP32-S3 image."""
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
ORIGINAL_SHA256 = '04a725fba76a2bbbfc302aa3c8283a6b2ee3b1d1c7e15ab89f5519185d092f70'
RELEASE_SHA256 = 'c3815e88e6edce96536e5dedb380e056b80ce9c937f2b4b5468e26f1774455ea'
VERSION_OFFSET = 0x30
CHECKSUM_OFFSET = 0x1AFFBF
DIGEST_OFFSET = 0x1AFFC0


def original_image():
    """Read the preserved CI payload, never a file the stamping test can alter."""
    payload = ROOT / 'firmware-payload.zip'
    if payload.exists():
        data = payload.read_bytes()
    else:
        data = b''.join(part.read_bytes() for part in sorted((ROOT / 'firmware-payload').glob('*.part')))
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        return archive.read('firmware/freslest_os_s3.bin')


def descriptor_version(data):
    return data[VERSION_OFFSET:VERSION_OFFSET + 32].split(b'\0', 1)[0].decode('ascii')


def repair_integrity(data):
    """Independently recompute the documented XOR/SHA footer for negative tests."""
    data = bytearray(data)
    position = 24
    checksum = 0xEF
    for _ in range(data[1]):
        _, size = struct.unpack_from('<II', data, position)
        position += 8
        for value in data[position:position + size]:
            checksum ^= value
        position += size
    checksum_offset = position | 15
    data[checksum_offset] = checksum
    data[checksum_offset + 1:] = hashlib.sha256(data[:checksum_offset + 1]).digest()
    return bytes(data)


class FirmwareVersionTests(unittest.TestCase):
    def test_preserved_original_reproduces_the_6_16_descriptor(self):
        data = original_image()
        self.assertEqual(hashlib.sha256(data).hexdigest(), ORIGINAL_SHA256)
        self.assertEqual(descriptor_version(data), '6.16')

    def test_shipped_image_reports_release_1_0(self):
        data = (ROOT / 'firmware' / 'freslest_os_s3.bin').read_bytes()
        self.assertEqual(descriptor_version(data), '1.0')

    def test_stamp_changes_only_version_and_required_integrity_bytes(self):
        from firmware_release import stamp_image
        original = original_image()
        stamped = stamp_image(original)
        self.assertEqual(descriptor_version(stamped), '1.0')
        self.assertEqual(hashlib.sha256(stamped).hexdigest(), RELEASE_SHA256)
        self.assertEqual(len(stamped), len(original))
        self.assertEqual(stamped[:VERSION_OFFSET], original[:VERSION_OFFSET])
        self.assertEqual(stamped[VERSION_OFFSET + 32:CHECKSUM_OFFSET],
                         original[VERSION_OFFSET + 32:CHECKSUM_OFFSET])
        self.assertEqual(stamped[DIGEST_OFFSET + 32:], original[DIGEST_OFFSET + 32:])
        self.assertEqual(stamped[VERSION_OFFSET:VERSION_OFFSET + 32], b'1.0' + bytes(29))

    def test_esptool_accepts_all_segments_checksum_and_digest(self):
        from esptool.bin_image import ESP32S3FirmwareImage
        from firmware_release import stamp_image
        old = ESP32S3FirmwareImage(io.BytesIO(original_image()))
        new = ESP32S3FirmwareImage(io.BytesIO(stamp_image(original_image())))
        self.assertEqual(new.chip_id, 9)
        self.assertEqual(new.checksum, new.calculate_checksum())
        self.assertEqual(new.stored_digest, new.calc_digest)
        self.assertEqual(new.segments[0].data[16:48], b'1.0' + bytes(29))
        self.assertEqual(len(new.segments), 7)
        for index, (before, after) in enumerate(zip(old.segments, new.segments)):
            self.assertEqual((before.addr, before.file_offs, len(before.data)),
                             (after.addr, after.file_offs, len(after.data)))
            if index == 0:
                self.assertEqual(before.data[:16] + before.data[48:],
                                 after.data[:16] + after.data[48:])
            else:
                self.assertEqual(before.data, after.data)

    def test_stamping_is_idempotent(self):
        from firmware_release import stamp_image
        stamped = stamp_image(original_image())
        self.assertEqual(stamp_image(stamped), stamped)

    def test_corrupt_image_is_rejected(self):
        from firmware_release import stamp_image
        original = original_image()
        corruptions = {
            'magic': (0, 0),
            'segment count': (1, 0),
            'segment length': (31, 0xFF),
            'chip id': (12, 0),
            'appended hash flag': (23, 0),
            'segment contents': (0x200, original[0x200] ^ 1),
            'checksum': (CHECKSUM_OFFSET, original[CHECKSUM_OFFSET] ^ 1),
            'digest': (DIGEST_OFFSET, original[DIGEST_OFFSET] ^ 1),
        }
        for name, (offset, value) in corruptions.items():
            with self.subTest(name=name):
                data = bytearray(original)
                data[offset] = value
                with self.assertRaises(ValueError):
                    stamp_image(bytes(data))

    def test_truncated_or_signed_images_are_rejected(self):
        from firmware_release import stamp_image
        original = original_image()
        for data in (original[:12], original[:-1], original + bytes(4096)):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                stamp_image(data)

    def test_valid_unrelated_image_is_rejected_by_pinned_provenance(self):
        from firmware_release import inspect_image, stamp_image
        data = bytearray(original_image())
        data[4] ^= 1  # A valid image with a different entry point is not our release.
        data = repair_integrity(data)
        self.assertEqual(inspect_image(data)['version'], '6.16')
        with self.assertRaisesRegex(ValueError, 'SHA256|provenance|recognized'):
            stamp_image(data)

    def test_valid_wrong_descriptor_is_rejected(self):
        from firmware_release import stamp_image
        for offset in (0x20, 0x24, 0x50):
            with self.subTest(offset=offset):
                data = bytearray(original_image())
                data[offset] ^= 1  # Descriptor magic, secure version, project name.
                with self.assertRaises(ValueError):
                    stamp_image(repair_integrity(data))

    def make_release(self, directory):
        original = original_image()
        original_manifest = (ROOT / 'firmware' / 'SHA256.json').read_text(encoding='utf-8')
        manifest = json.loads(original_manifest)
        current_hash = manifest['firmware/freslest_os_s3.bin']
        original_manifest = original_manifest.replace(current_hash, ORIGINAL_SHA256)
        (directory / 'freslest_os_s3.bin').write_bytes(original)
        (directory / 'SHA256.json').write_text(original_manifest, encoding='utf-8')
        (directory / 'freslest_doom_s3.bin').write_bytes(b'unchanged Doom sentinel')
        return original_manifest

    def test_release_updates_only_os_manifest_value_and_writes_evidence(self):
        from firmware_release import stamp_release
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            old_manifest = self.make_release(directory)
            report = stamp_release(directory)
            image = (directory / 'freslest_os_s3.bin').read_bytes()
            expected_hash = hashlib.sha256(image).hexdigest()
            self.assertEqual(descriptor_version(image), '1.0')
            self.assertEqual((directory / 'SHA256.json').read_text(encoding='utf-8'),
                             old_manifest.replace(ORIGINAL_SHA256, expected_hash))
            self.assertEqual((directory / 'freslest_doom_s3.bin').read_bytes(), b'unchanged Doom sentinel')
            self.assertEqual(report['release_sha256'], expected_hash)
            self.assertEqual(report['method'], 'application-descriptor release stamping')
            self.assertFalse(report['source_rebuild'])
            self.assertTrue(report['outside_metadata_unchanged'])
            self.assertEqual(json.loads((directory / 'VERSION_REPORT.json').read_text(encoding='utf-8')), report)
            snapshot = {path.name: path.read_bytes() for path in directory.iterdir()}
            self.assertEqual(stamp_release(directory), report)
            self.assertEqual({path.name: path.read_bytes() for path in directory.iterdir()}, snapshot)

    def test_failed_manifest_validation_does_not_write_any_file(self):
        from firmware_release import stamp_release
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.make_release(directory)
            path = directory / 'SHA256.json'
            path.write_text(path.read_text(encoding='utf-8').replace(ORIGINAL_SHA256, '0' * 64), encoding='utf-8')
            snapshot = {path.name: path.read_bytes() for path in directory.iterdir()}
            with self.assertRaises(ValueError):
                stamp_release(directory)
            self.assertEqual({path.name: path.read_bytes() for path in directory.iterdir()}, snapshot)

    def test_rerun_recovers_manifest_after_interrupted_os_replacement(self):
        from firmware_release import stamp_image, stamp_release
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.make_release(directory)
            (directory / 'freslest_os_s3.bin').write_bytes(stamp_image(original_image()))
            stamp_release(directory)
            manifest = json.loads((directory / 'SHA256.json').read_text(encoding='utf-8'))
            self.assertEqual(manifest['firmware/freslest_os_s3.bin'],
                             hashlib.sha256((directory / 'freslest_os_s3.bin').read_bytes()).hexdigest())

    def test_non_string_manifest_hash_is_rejected_before_writing(self):
        from firmware_release import stamp_release
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.make_release(directory)
            path = directory / 'SHA256.json'
            manifest = json.loads(path.read_text(encoding='utf-8'))
            manifest['firmware/freslest_os_s3.bin'] = []
            path.write_text(json.dumps(manifest), encoding='utf-8')
            snapshot = {path.name: path.read_bytes() for path in directory.iterdir()}
            try:
                stamp_release(directory)
            except Exception as error:
                self.assertIsInstance(error, ValueError)
            else:
                self.fail('An invalid manifest hash was accepted')
            self.assertEqual({path.name: path.read_bytes() for path in directory.iterdir()}, snapshot)

    def test_cli_stamps_a_real_release_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.make_release(directory)
            result = subprocess.run([sys.executable, str(ROOT / 'firmware_release.py'), str(directory)],
                                    text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('1.0', result.stdout)
            self.assertEqual(descriptor_version((directory / 'freslest_os_s3.bin').read_bytes()), '1.0')

    def test_other_shipped_firmware_files_match_the_original_payload(self):
        payload = ROOT / 'firmware-payload.zip'
        data = payload.read_bytes() if payload.exists() else b''.join(
            part.read_bytes() for part in sorted((ROOT / 'firmware-payload').glob('*.part')))
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            for entry in archive.namelist():
                if entry.startswith('firmware/') and not entry.endswith(('/', '/SHA256.json', '/freslest_os_s3.bin')):
                    with self.subTest(name=entry):
                        self.assertEqual((ROOT / entry).read_bytes(), archive.read(entry))


if __name__ == '__main__':
    unittest.main()
