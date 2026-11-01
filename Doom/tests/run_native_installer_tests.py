"""Exercise the real installer repacker against a small complete CArchive.

No ESP-IDF checkout, Windows runtime, or production installer is required.
The opaque runtime/PYZ bytes, all three profiles, and 112 SD sounds are checked
independently of the production archive reader and writer.
"""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[2]
REPACK = ROOT / 'tools/repack_doom_installer.py'
COOKIE = struct.Struct('!8sIIII64s')
TOC = struct.Struct('!iIIIBc')
OS_ENTRY = 'firmware\\vga\\freslest_os_s3.bin'
DOOM_ENTRY = 'firmware\\vga\\freslest_doom_s3.bin'
HASH_ENTRY = 'firmware\\vga\\SHA256.json'
REPORT_ENTRY = 'firmware\\VERSION_REPORT.json'
SOURCE_COMMIT = '0123456789abcdef0123456789abcdef01234567'
OS_MARKERS = (b'FRESLEST_VIDEO:VGA25175_25175000', b'FRESLEST_LCD:ILI9341',
              b'FRESLEST_UPDATE:OS_AND_DOOM_V3')
DOOM_MARKERS = (b'DOOM_VGA:PRIVATE_320x240', b'FRESLEST_VIDEO:VGA25175_25175000',
                b'DOOM_VGA:NATIVE_RGB332_WORDS', b'DOOM_BOOT reset=')


def esp_image(project, markers, tag=b'new', chip=9, flash=0x40):
    """Build an independently checksummed, SHA-appended ESP application image."""
    descriptor = bytearray(256)
    struct.pack_into('<I', descriptor, 0, 0xabcd5432)
    descriptor[16:16 + len(b'7.0.0\0')] = b'7.0.0\0'
    encoded = project.encode('ascii') + b'\0'
    descriptor[48:48 + len(encoded)] = encoded
    segment = descriptor + b'\0'.join(markers) + b'\0' + tag + bytes(range(256))
    header = bytearray(24)
    header[:4] = bytes([0xe9, 1, 2, flash])
    struct.pack_into('<H', header, 12, chip)
    header[23] = 1
    image = header + struct.pack('<II', 0x3c020020, len(segment)) + segment
    checksum = 0xef
    for byte in segment:
        checksum ^= byte
    while (len(image) + 1) % 16:
        image.append(0)
    image.append(checksum)
    return bytes(image) + hashlib.sha256(image).digest()


def encode_json(value):
    return (json.dumps(value, indent=2) + '\n').encode()


def write_fixture(path):
    records = []

    def add(name, data, compressed=1, kind=b'x'):
        records.append((name, zlib.compress(data, 1) if compressed else data,
                        len(data), compressed, kind))

    add('python311.dll', b'opaque Windows Python runtime\0' * 9, kind=b'b')
    add('runtime\\dependency.dll', bytes(range(256)) * 3, kind=b'b')
    add('installer', b'opaque compiled installer code\0' * 11, kind=b's')
    report = {'git': {'commit': 'base-installer-commit', 'dirty': False},
              'changes': ['Preserved complete installer'], 'profiles': {}}
    for profile in ('ili9341', 'ili9488', 'vga'):
        images = {
            'bootloader.bin': b'preserved bootloader ' + profile.encode(),
            'partition-table.bin': b'preserved partition table ' + profile.encode(),
            'freslest_os_s3.bin': esp_image('freslest_os_s3', OS_MARKERS[:2],
                                          tag=('old-' + profile).encode()),
            'freslest_doom_s3.bin': esp_image('freslest_doom_s3', DOOM_MARKERS,
                                            tag=('old-' + profile).encode()),
            'filesystem.bin': b'preserved filesystem ' + profile.encode(),
        }
        hashes = {}
        report['profiles'][profile] = {}
        for filename, data in images.items():
            add(f'firmware\\{profile}\\{filename}', data,
                compressed=0 if filename == 'partition-table.bin' else 1)
            digest = hashlib.sha256(data).hexdigest()
            hashes['firmware/' + filename] = digest
            report['profiles'][profile][filename] = {'bytes': len(data), 'sha256': digest}
        add(f'firmware\\{profile}\\SHA256.json', encode_json(hashes))
    add(REPORT_ENTRY, encode_json(report))
    audio_hashes = {}
    for number in range(1, 113):
        data = b'ID3 fixture sound ' + str(number).encode() + bytes(range(16))
        add(f'audio_pack\\MP3\\{number:04}.mp3', data, compressed=number % 2)
        audio_hashes[f'MP3/{number:04}.mp3'] = hashlib.sha256(data).hexdigest()
    add('audio_pack\\SHA256.json', encode_json(audio_hashes))
    add('installer.exe.manifest', b'<assembly>opaque runtime manifest</assembly>')
    add('PYZ.pyz', b'PYZ\0' + bytes(range(256)) * 5, compressed=0, kind=b'z')
    prefix = b'MZ' + bytes(range(256)) * 4 + b'opaque PE bootloader prefix'
    body = bytearray()
    table = bytearray()
    for name, packed, size, compressed, kind in records:
        encoded = name.encode() + b'\0'
        length = ((TOC.size + len(encoded) + 15) // 16) * 16
        table += TOC.pack(length, len(body), len(packed), size, compressed, kind)
        table += encoded.ljust(length - TOC.size, b'\0')
        body += packed
    cookie = COOKIE.pack(b'MEI\x0c\x0b\x0a\x0b\x0e', len(body) + len(table) + COOKIE.size,
                         len(body), len(table), 311, b'python311.dll')
    path.write_bytes(prefix + body + table + cookie)


def read_fixture(path):
    """Inspect packed bytes and metadata without the production parser."""
    data = path.read_bytes()
    magic, size, table_offset, table_size, pyver, pylib = COOKIE.unpack(data[-COOKIE.size:])
    assert magic == b'MEI\x0c\x0b\x0a\x0b\x0e'
    start = len(data) - size
    table = data[start + table_offset:start + table_offset + table_size]
    records = {}
    cursor = 0
    while cursor < len(table):
        length, pos, packed_size, raw_size, compressed, kind = TOC.unpack_from(table, cursor)
        name = table[cursor + TOC.size:cursor + length].rstrip(b'\0').decode()
        packed = data[start + pos:start + pos + packed_size]
        records[name] = (packed, raw_size, compressed, kind)
        cursor += length
    return (data[:start], pyver, pylib), records


def unpack(record):
    packed, size, compressed, _ = record
    raw = zlib.decompress(packed) if compressed else packed
    assert len(raw) == size
    return raw


class NativeInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.base = self.directory / 'base.exe'
        write_fixture(self.base)
        self.doom = self.directory / 'doom.bin'
        self.doom.write_bytes(esp_image('freslest_doom_s3', DOOM_MARKERS))
        self.os = self.directory / 'os.bin'
        self.os.write_bytes(esp_image('freslest_os_s3', OS_MARKERS))
        self.manifest_data = {
            'source_commit': SOURCE_COMMIT,
            'sha256': hashlib.sha256(self.doom.read_bytes()).hexdigest(),
            'bytes': self.doom.stat().st_size, 'vga_signal': [640, 480],
            'target_clock_hz': 25175000,
        }
        self.manifest = self.directory / 'doom-manifest.json'
        self.manifest.write_bytes(encode_json(self.manifest_data))
        self.output = self.directory / 'result.exe'

    def repack(self, include_os=True):
        command = [sys.executable, str(REPACK), '--base', str(self.base),
                   '--payload', str(self.doom), '--manifest', str(self.manifest),
                   '--output', str(self.output)]
        if include_os:
            command += ['--os', str(self.os)]
        return subprocess.run(command, capture_output=True, text=True)

    def assert_archive_replacement(self, result, changed):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        summary = json.loads(result.stdout)
        prefix_before, before = read_fixture(self.base)
        prefix_after, after = read_fixture(self.output)
        self.assertEqual(prefix_after, prefix_before)
        self.assertEqual(list(after), list(before))
        self.assertEqual({name for name in before if before[name] != after[name]}, changed)
        for name in before.keys() - changed:
            self.assertEqual(after[name], before[name], name)
        self.assertEqual(summary['changed_entries'], sorted(changed))
        self.assertEqual(summary['unchanged_entries'], len(before) - len(changed))
        self.assertEqual(summary['sd_audio_tracks'], 112)
        self.assertEqual(summary['sd_audio_hashes_verified'], 112)
        paths = []
        for name, (_, _, _, kind) in after.items():
            if kind in (b'b', b'x', b'Z'):
                self.assertNotIn('/', name)
                self.assertNotIn(':', name)
                self.assertTrue(all(part not in ('', '.', '..') for part in name.split('\\')))
                paths.append(name)
        self.assertEqual(summary['windows_extraction_paths'], len(paths))
        for profile in ('ili9341', 'ili9488', 'vga'):
            hashes = json.loads(unpack(after[f'firmware\\{profile}\\SHA256.json']))
            for filename, digest in hashes.items():
                raw = unpack(after[f'firmware\\{profile}\\{filename.rsplit("/", 1)[-1]}'])
                self.assertEqual(hashlib.sha256(raw).hexdigest(), digest)
        audio_hashes = json.loads(unpack(after['audio_pack\\SHA256.json']))
        self.assertEqual(set(audio_hashes), {f'MP3/{number:04}.mp3' for number in range(1, 113)})
        for name, digest in audio_hashes.items():
            raw = unpack(after['audio_pack\\' + name.replace('/', '\\')])
            self.assertEqual(hashlib.sha256(raw).hexdigest(), digest)
        report = json.loads(unpack(after[REPORT_ENTRY]))
        original = json.loads(unpack(before[REPORT_ENTRY]))
        self.assertEqual(report['git'], original['git'])
        self.assertEqual(report['profiles']['ili9341'], original['profiles']['ili9341'])
        self.assertEqual(report['profiles']['ili9488'], original['profiles']['ili9488'])
        self.assertEqual(unpack(after[DOOM_ENTRY]), self.doom.read_bytes())
        self.assertEqual(report['doom_vga_build'], self.manifest_data)
        self.assertEqual(report['profiles']['vga']['freslest_doom_s3.bin'], {
            'bytes': self.doom.stat().st_size,
            'sha256': hashlib.sha256(self.doom.read_bytes()).hexdigest(),
        })
        return before, after, report

    def test_replaces_vga_os_and_doom_preserving_every_other_packed_entry(self):
        _, after, report = self.assert_archive_replacement(
            self.repack(), {OS_ENTRY, DOOM_ENTRY, HASH_ENTRY, REPORT_ENTRY})
        self.assertEqual(unpack(after[OS_ENTRY]), self.os.read_bytes())
        self.assertEqual(report['profiles']['vga']['freslest_os_s3.bin'], {
            'bytes': self.os.stat().st_size,
            'sha256': hashlib.sha256(self.os.read_bytes()).hexdigest(),
        })
        self.assertEqual(report['native_doom_update'], {
            'format_version': 3, 'updater_min': 3, 'bootstrap_format_version': 2,
            'source_commit': SOURCE_COMMIT,
        })

    def test_doom_only_preserves_vga_os_and_changes_exactly_three_entries(self):
        self.assert_archive_replacement(
            self.repack(include_os=False), {DOOM_ENTRY, HASH_ENTRY, REPORT_ENTRY})

    def test_rejects_os_without_native_update_capability_before_writing_output(self):
        self.os.write_bytes(esp_image('freslest_os_s3', OS_MARKERS[:2]))
        result = self.repack()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('FRESLEST_UPDATE:OS_AND_DOOM_V3', result.stderr)
        self.assertFalse(self.output.exists())

    def test_rejects_corrupt_os_checksum_before_writing_output(self):
        image = bytearray(self.os.read_bytes())
        image[-33] ^= 1
        # Preserve the outer SHA to isolate the ESP segment-checksum rejection.
        image[-32:] = hashlib.sha256(image[:-32]).digest()
        self.os.write_bytes(image)
        result = self.repack()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('checksum', result.stderr.lower())
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
