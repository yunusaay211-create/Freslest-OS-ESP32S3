"""Check display-specific native packages without rebuilding released Doom images."""
from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile

from update_target_fixtures import DOOM_MARKERS, DOOM_PROJECT, OS_PROJECT, esp_image

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tools/make_doom_update_zip.py'
SPEC = importlib.util.spec_from_file_location('profile_package_builder', SCRIPT)
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


def lcd_markers(profile, project, native=False):
    values = [b'FRESLEST_LCD:ILI' + profile[-4:].encode()]
    if project == OS_PROJECT:
        values.append(b'FRESLEST_UPDATE:OS_AND_DOOM_V3')
    elif native:
        values.extend(DOOM_MARKERS[2:])
    return values


class UpdateProfileTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.output = self.directory / 'update.zip'

    def image(self, project, profile='vga', native=False, extra=()):
        markers = None if profile == 'vga' else lcd_markers(profile, project, native)
        if extra:
            if markers is None:
                from update_target_fixtures import OS_MARKERS
                markers = list(DOOM_MARKERS if project == DOOM_PROJECT else OS_MARKERS)
            markers = [*markers, *extra]
        path = self.directory / (profile + '-' + project + '.bin')
        path.write_bytes(esp_image(project, markers=markers))
        return path

    def checked(self, path, project=DOOM_PROJECT, **kwargs):
        return builder.checked_image(path, project, 0xe0000 if project == DOOM_PROJECT else 0x1b0000, **kwargs)

    def test_default_api_remains_strict_vga_with_legacy_manifest_shape(self):
        builder.make_package(self.image(OS_PROJECT), self.image(DOOM_PROJECT), self.output)
        with zipfile.ZipFile(self.output) as package:
            self.assertEqual(len(package.infolist()), 5)
            for name in ('freslest-update.json', 'freslest-doom-update.json'):
                self.assertNotIn('display', json.loads(package.read(name)))
        with self.assertRaises(ValueError):
            self.checked(self.image(DOOM_PROJECT, 'ili9341'))

    def test_lcd_dual_packages_preserve_os_display_and_match_both_boards(self):
        for profile in ('ili9488', 'ili9341'):
            os_image, doom_image = self.image(OS_PROJECT, profile), self.image(DOOM_PROJECT, profile)
            builder.make_package(os_image, doom_image, self.output, profile=profile, reuse_doom=True)
            with zipfile.ZipFile(self.output) as package:
                self.assertEqual(len(package.infolist()), 5)
                for name, display, version in [('freslest-update.json', 'preserve', 2),
                                                ('freslest-doom-update.json', 'lcd', 3)]:
                    value = json.loads(package.read(name))
                    self.assertEqual(value['board'], 'esp32s3-n16r8-' + profile + '-buttons')
                    self.assertEqual(value['display'], display)
                    self.assertEqual(value['format_version'], version)
                self.assertEqual(package.read('firmware/freslest_doom_s3.bin'), doom_image.read_bytes())
                self.assertIsNone(package.testzip())

    def test_released_lcd_doom_requires_explicit_reuse(self):
        path = self.image(DOOM_PROJECT, 'ili9488')
        with self.assertRaises(ValueError):
            self.checked(path, profile='ili9488')
        self.assertEqual(self.checked(path, profile='ili9488', require_native_doom=False), path.read_bytes())

    def test_new_lcd_doom_can_use_strict_native_validation(self):
        for profile in ('ili9488', 'ili9341'):
            path = self.image(DOOM_PROJECT, profile, native=True)
            self.assertEqual(self.checked(path, profile=profile), path.read_bytes())

    def test_lcd_marker_is_exact_not_a_prefix(self):
        for marker in (b'FRESLEST_LCD:ILI93410', b'FRESLEST_LCD:ILI9488', b'FRESLEST_LCD:OTHER'):
            path = self.directory / 'invalid.bin'
            path.write_bytes(esp_image(DOOM_PROJECT, markers=(marker,)))
            with self.assertRaises(ValueError):
                self.checked(path, profile='ili9341', require_native_doom=False)

    def test_conflicting_lcd_markers_are_rejected(self):
        for profile, extra in [('ili9488', b'FRESLEST_LCD:ILI9341'),
                               ('ili9341', b'FRESLEST_LCD:ILI9488'),
                               ('vga', b'FRESLEST_LCD:ILI9488')]:
            with self.assertRaises(ValueError):
                self.checked(self.image(DOOM_PROJECT, profile, extra=(extra,)),
                             profile=profile, require_native_doom=False)

    def test_lcd_images_reject_every_vga_preset(self):
        for marker in (b'FRESLEST_VIDEO:VGA25175_25175000', b'FRESLEST_VIDEO:VGA252_25200000',
                       b'FRESLEST_VIDEO:VGA_OTHER'):
            with self.assertRaises(ValueError):
                self.checked(self.image(DOOM_PROJECT, 'ili9341', extra=(marker,)),
                             profile='ili9341', require_native_doom=False)

    def test_vga_requires_only_the_current_clock_even_with_reuse(self):
        for markers in ((m for m in DOOM_MARKERS if not m.startswith(b'FRESLEST_VIDEO:')),
                        (*DOOM_MARKERS, b'FRESLEST_VIDEO:VGA252_25200000'),
                        (*DOOM_MARKERS, b'FRESLEST_VIDEO:VGA25175_251750001')):
            path = self.directory / 'invalid.bin'
            path.write_bytes(esp_image(DOOM_PROJECT, markers=markers))
            with self.assertRaises(ValueError):
                self.checked(path, require_native_doom=False)

    def test_vga_reuse_never_relaxes_native_doom_markers(self):
        for marker in DOOM_MARKERS[2:]:
            path = self.directory / 'invalid.bin'
            path.write_bytes(esp_image(DOOM_PROJECT, markers=[m for m in DOOM_MARKERS if m != marker]))
            with self.assertRaises(ValueError):
                self.checked(path, require_native_doom=False)

    def test_os_always_requires_the_native_update_capability(self):
        for profile in ('ili9488', 'ili9341'):
            path = self.directory / 'invalid.bin'
            path.write_bytes(esp_image(OS_PROJECT, markers=lcd_markers(profile, DOOM_PROJECT)))
            with self.assertRaisesRegex(ValueError, 'OS_AND_DOOM_V3'):
                self.checked(path, OS_PROJECT, profile=profile, require_native_doom=False)

    def test_profile_options_keep_esp_integrity_and_slot_guards(self):
        original = self.image(DOOM_PROJECT, 'ili9488').read_bytes()
        damaged = []
        for offset in (0, 3, 12, 23, 32, len(original) - 33, len(original) - 1):
            data = bytearray(original)
            data[offset] ^= 1 if offset != 3 else 0x10
            damaged.append(data)
        data = bytearray(original)
        struct.pack_into('<I', data, 28, 0xfffffffc)
        damaged.append(data)
        damaged.extend((original[:280], esp_image(DOOM_PROJECT, markers=lcd_markers('ili9488', DOOM_PROJECT),
                                                  total_size=0xe0010)))
        for data in damaged:
            path = self.directory / 'invalid.bin'
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                self.checked(path, profile='ili9488', require_native_doom=False)
        with self.assertRaises(ValueError):
            self.checked(self.image(DOOM_PROJECT), profile='unknown')

    def test_doom_only_vga_package_has_three_entries_and_explicit_display(self):
        path = self.image(DOOM_PROJECT)
        builder.make_doom_package(path, self.output)
        with zipfile.ZipFile(self.output) as package:
            self.assertEqual(set(package.namelist()), {'freslest-doom-update.json',
                             'firmware/freslest_doom_s3.bin', 'README.txt'})
            value = json.loads(package.read('freslest-doom-update.json'))
            self.assertEqual((value['target'], value['display'], value['format_version']), ('doom', 'vga', 3))
            self.assertEqual(value['image']['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())

    def test_doom_only_lcd_reuse_keeps_released_image_bytes(self):
        path = self.image(DOOM_PROJECT, 'ili9488')
        builder.make_doom_package(path, self.output, profile='ili9488', reuse_doom=True)
        with zipfile.ZipFile(self.output) as package:
            value = json.loads(package.read('freslest-doom-update.json'))
            self.assertEqual(value['display'], 'lcd')
            self.assertEqual(value['board'], 'esp32s3-n16r8-ili9488-buttons')
            self.assertEqual(package.read('firmware/freslest_doom_s3.bin'), path.read_bytes())

    def test_cli_doom_only_needs_no_os_and_invalid_dual_keeps_existing_output(self):
        path = self.image(DOOM_PROJECT)
        command = [sys.executable, str(SCRIPT), '--doom', str(path), '--output', str(self.output)]
        result = subprocess.run([*command, '--doom-only'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        original = self.output.read_bytes()
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--os', result.stderr)
        self.assertEqual(self.output.read_bytes(), original)
        with self.assertRaises(ValueError):
            builder.make_package(self.image(OS_PROJECT), self.image(DOOM_PROJECT, 'ili9341'), self.output)
        self.assertEqual(self.output.read_bytes(), original)


if __name__ == '__main__':
    unittest.main(verbosity=2)
