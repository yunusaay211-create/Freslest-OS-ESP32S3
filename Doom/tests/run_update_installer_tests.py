"""Check seven-entry OS replacement while every released Doom/runtime byte stays intact."""
from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib

from run_native_installer_tests import COOKIE, TOC, SOURCE_COMMIT, encode_json, read_fixture, unpack, write_fixture
from update_target_fixtures import DOOM_PROJECT, OS_PROJECT, esp_image
from run_update_profile_tests import lcd_markers

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tools/repack_update_installer.py'
PROFILES = ('ili9488', 'ili9341', 'vga')
REPORT = 'firmware\\VERSION_REPORT.json'
CHANGED = {REPORT} | {f'firmware\\{profile}\\{name}' for profile in PROFILES
                     for name in ('freslest_os_s3.bin', 'SHA256.json')}


def rewrite_fixture(path, records, prefix):
    bootloader, pyver, pylib = prefix
    body, table = bytearray(), bytearray()
    for name, (packed, size, compressed, kind) in records.items():
        encoded = name.encode() + b'\0'
        length = (TOC.size + len(encoded) + 15) // 16 * 16
        table += TOC.pack(length, len(body), len(packed), size, compressed, kind)
        table += encoded.ljust(length - TOC.size, b'\0')
        body += packed
    path.write_bytes(bootloader + body + table + COOKIE.pack(b'MEI\x0c\x0b\x0a\x0b\x0e',
                     len(body) + len(table) + COOKIE.size, len(body), len(table), pyver, pylib))


class UpdateInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.base, self.output = self.directory / 'base.exe', self.directory / 'new.exe'
        write_fixture(self.base)
        self.profiles = self.directory / 'profiles'
        for profile in PROFILES:
            folder = self.profiles / profile
            folder.mkdir(parents=True)
            markers = None if profile == 'vga' else lcd_markers(profile, OS_PROJECT)
            (folder / 'freslest_os_s3.bin').write_bytes(esp_image(OS_PROJECT, markers=markers))

    def repack(self, source_commit=SOURCE_COMMIT):
        return subprocess.run([sys.executable, str(SCRIPT), '--base', str(self.base),
                               '--profiles', str(self.profiles), '--source-commit', source_commit,
                               '--output', str(self.output)], capture_output=True, text=True)

    def replace_record(self, name, raw):
        prefix, records = read_fixture(self.base)
        _, _, compressed, kind = records[name]
        records[name] = (zlib.compress(raw, 1) if compressed else raw, len(raw), compressed, kind)
        rewrite_fixture(self.base, records, prefix)

    def assert_rejected(self):
        result = self.repack()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse(self.output.exists())
        self.assertFalse(list(self.directory.glob('new.exe.*.tmp')))
        return result

    def test_replaces_exactly_seven_entries_and_preserves_all_packed_doom_runtime_audio(self):
        result = self.repack()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        summary = json.loads(result.stdout)
        prefix_before, before = read_fixture(self.base)
        prefix_after, after = read_fixture(self.output)
        self.assertEqual(prefix_before, prefix_after)
        self.assertEqual(list(before), list(after))
        self.assertEqual({name for name in before if before[name] != after[name]}, CHANGED)
        self.assertEqual(summary['changed_entries'], sorted(CHANGED))
        self.assertEqual(summary['unchanged_entries'], len(before) - 7)
        self.assertEqual(summary['sd_audio_hashes_verified'], 112)
        for name in before.keys() - CHANGED:
            self.assertEqual(before[name], after[name], name)
        original_report = json.loads(unpack(before[REPORT]))
        report = json.loads(unpack(after[REPORT]))
        self.assertEqual(report['git'], original_report['git'])
        for profile in PROFILES:
            name = f'firmware\\{profile}\\freslest_os_s3.bin'
            intended = (self.profiles / profile / 'freslest_os_s3.bin').read_bytes()
            self.assertEqual(unpack(after[name]), intended)
            self.assertEqual(report['profiles'][profile]['freslest_os_s3.bin'],
                             {'bytes': len(intended), 'sha256': hashlib.sha256(intended).hexdigest()})
            self.assertEqual(report['profiles'][profile]['freslest_doom_s3.bin'],
                             original_report['profiles'][profile]['freslest_doom_s3.bin'])
            hashes = json.loads(unpack(after[f'firmware\\{profile}\\SHA256.json']))
            for filename, expected in hashes.items():
                self.assertEqual(hashlib.sha256(unpack(after[f'firmware\\{profile}\\' + filename.rsplit('/', 1)[-1]])).hexdigest(), expected)
        audio_hashes = json.loads(unpack(after['audio_pack\\SHA256.json']))
        self.assertEqual(len(audio_hashes), 112)
        for name, expected in audio_hashes.items():
            self.assertEqual(hashlib.sha256(unpack(after['audio_pack\\' + name.replace('/', '\\')])).hexdigest(), expected)
        paths = [name for name, record in after.items() if record[3] in (b'b', b'x', b'Z')]
        self.assertEqual(summary['windows_extraction_paths'], len(paths))
        self.assertTrue(all('/' not in name and ':' not in name and
                            all(part not in ('', '.', '..') for part in name.split('\\')) for name in paths))
        self.assertEqual(report['native_os_doom_update']['source_commit'], SOURCE_COMMIT)
        self.assertEqual(report['native_os_doom_update']['doom_images'], 'unchanged')

    def test_wrong_lcd_profile_is_rejected_before_output(self):
        path = self.profiles / 'ili9488/freslest_os_s3.bin'
        path.write_bytes(esp_image(OS_PROJECT, markers=lcd_markers('ili9341', OS_PROJECT)))
        self.assert_rejected()

    def test_os_without_native_capability_is_rejected_before_output(self):
        path = self.profiles / 'ili9341/freslest_os_s3.bin'
        path.write_bytes(esp_image(OS_PROJECT, markers=lcd_markers('ili9341', DOOM_PROJECT)))
        self.assertIn('OS_AND_DOOM_V3', self.assert_rejected().stderr)

    def test_corrupt_os_checksum_keeps_existing_output_untouched(self):
        path = self.profiles / 'vga/freslest_os_s3.bin'
        data = bytearray(path.read_bytes())
        data[-33] ^= 1
        data[-32:] = hashlib.sha256(data[:-32]).digest()
        path.write_bytes(data)
        self.output.write_bytes(b'existing delivery')
        result = self.repack()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('checksum', result.stderr.lower())
        self.assertEqual(self.output.read_bytes(), b'existing delivery')

    def test_base_firmware_hashes_and_complete_manifest_coverage_are_required(self):
        name = 'firmware\\ili9488\\SHA256.json'
        _, records = read_fixture(self.base)
        hashes = json.loads(unpack(records[name]))
        for invalid in ({**hashes, 'firmware/freslest_doom_s3.bin': '0' * 64},
                        {key: value for key, value in hashes.items() if key != 'firmware/bootloader.bin'}):
            self.replace_record(name, encode_json(invalid))
            self.assert_rejected()

    def test_base_audio_hashes_are_required_before_output(self):
        self.replace_record('audio_pack\\MP3\\0112.mp3', b'corrupt sound')
        self.assert_rejected()

    def test_invalid_windows_extraction_paths_are_rejected(self):
        prefix, records = read_fixture(self.base)
        records['runtime/dependency.dll'] = records.pop('runtime\\dependency.dll')
        rewrite_fixture(self.base, records, prefix)
        self.assert_rejected()

    def test_failed_staged_validation_never_publishes_output(self):
        sys.path.insert(0, str(ROOT / 'tools'))
        self.addCleanup(lambda: sys.path.remove(str(ROOT / 'tools')))
        spec = importlib.util.spec_from_file_location('update_installer_repacker', SCRIPT)
        repacker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(repacker)
        actual_check = repacker.check_archive
        calls = 0

        def fail_staged(path, **kwargs):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise ValueError('Simulated staged archive rejection')
            return actual_check(path, **kwargs)

        self.output.write_bytes(b'existing delivery')
        with mock.patch.object(repacker, 'check_archive', side_effect=fail_staged):
            with self.assertRaisesRegex(ValueError, 'staged archive rejection'):
                repacker.repack(self.base, self.profiles, SOURCE_COMMIT, self.output)
        self.assertEqual(self.output.read_bytes(), b'existing delivery')
        self.assertFalse(list(self.directory.glob('new.exe.*.tmp')))
        with self.assertRaises(ValueError):
            repacker.repack(self.base, self.profiles, SOURCE_COMMIT, self.base)
        with self.assertRaises(ValueError):
            repacker.repack(self.base, self.profiles, 'invalid', self.output)


if __name__ == '__main__':
    unittest.main(verbosity=2)
