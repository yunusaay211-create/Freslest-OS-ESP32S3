import hashlib
import os
import json
from pathlib import Path
import queue
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'installer'))
import installer as app


@unittest.skipIf(os.name != 'nt' and not os.environ.get('DISPLAY'), 'Requires a desktop or Xvfb')
class DisplayWizardTests(unittest.TestCase):
    def test_display_page_preserves_selection_and_passes_it_to_worker(self):
        window = app.Installer('en')
        try:
            self.assertEqual(window.display.get(), 'ili9488')
            window.displays()
            # Native Windows may not map the new top-level on idle alone.
            # Measure geometry only after its real event loop has mapped it.
            window.deiconify()
            window.geometry('690x560')
            window.update()
            self.assertGreaterEqual(window.winfo_height(), 500)
            self.assertIn('1 / 3   DISPLAYS', [str(w.cget('text')) for w in window.winfo_children() if isinstance(w, app.tk.Label)])
            choices = [w for w in window.winfo_children() if isinstance(w, app.tk.Radiobutton)]
            self.assertEqual([w.cget('value') for w in choices], ['ili9488', 'ili9341', 'vga'])
            self.assertTrue(all(w.winfo_y() + w.winfo_height() <= 560 for w in choices))
            self.assertTrue(all(w.winfo_y() + w.winfo_height() <= 560 for w in window.winfo_children()))
            hint = next(w for w in window.winfo_children() if isinstance(w, app.tk.Label) and '800×525' in str(w.cget('text')))
            buttons = next(w for w in window.winfo_children() if isinstance(w, app.tk.Frame))
            self.assertLessEqual(hint.winfo_y() + hint.winfo_height(), buttons.winfo_y())
            next(w for w in choices if w.cget('value') == 'vga').invoke()
            window.port.set('COM7')
            window.baud.set('115200')
            window.ports()
            window.displays()
            self.assertEqual(window.display.get(), 'vga')
            window.language_code = 'tr'
            window.wizard()
            self.assertEqual(window.display.get(), 'vga')
            with mock.patch.object(app.threading, 'Thread') as thread:
                window.flash('COM7')
            self.assertEqual(thread.call_args.kwargs['args'], ('COM7', 115200, 'vga'))
        finally:
            # ttk's running progress bar owns a Tcl-list timer; stop it through
            # the widget API before cancelling Python callbacks. Always destroy
            # this interpreter so later language tests get a fresh default root.
            try:
                progress = window.__dict__.get('progress')
                if progress is not None:
                    progress.stop()
                for token in window.tk.splitlist(window.tk.call('after', 'info')):
                    window.after_cancel(token)
            finally:
                window.destroy()


class DisplayInstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.patch = mock.patch.object(app, 'FW', self.root)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def payload(self, model, folder=None):
        directory = self.root / (folder or model)
        directory.mkdir(exist_ok=True)
        manifest = {}
        for _, name, _ in app.ITEMS:
            tags = {'vga': 'FRESLEST_LCD:ILI9341 FRESLEST_VIDEO:VGA25175_25175000'}
            data = (tags.get(model, f'FRESLEST_LCD:{model.upper()}') if 's3.bin' in name else name).encode()
            (directory / name).write_bytes(data)
            manifest['firmware/' + name] = hashlib.sha256(data).hexdigest()
        (directory / 'SHA256.json').write_text(json.dumps(manifest))
        return directory

    def test_each_display_selects_its_own_verified_os_and_doom(self):
        for model in ('ili9488', 'ili9341', 'vga'):
            self.payload(model)
        for model in ('ili9488', 'ili9341', 'vga'):
            result = app.validated_payload('en', model)
            self.assertEqual(len(result), 7)
            self.assertTrue(all(path.parent == self.root / model for _, path, _, _ in result))

    def test_missing_9341_never_falls_back_to_9488(self):
        self.payload('ili9488')
        with self.assertRaisesRegex(RuntimeError, 'ILI9341'):
            app.validated_payload('en', 'ili9341')

    def test_swapped_firmware_is_rejected_even_with_matching_hashes(self):
        self.payload('ili9488', folder='ili9341')
        with self.assertRaisesRegex(RuntimeError, 'ILI9341'):
            app.validated_payload('en', 'ili9341')

    def test_unknown_model_is_rejected(self):
        with self.assertRaises(ValueError):
            app.validated_payload('en', '../ili9341')

    def test_profile_order_and_titles(self):
        self.assertEqual(list(app.DISPLAY_MODELS), ['ili9488', 'ili9341', 'vga'])
        self.assertEqual(app.DISPLAY_MODELS['vga'], 'VGA RGB332 — 25.175 MHz (640×480 / 59.94 Hz)')

    def test_missing_vga_profiles_never_fall_back(self):
        self.payload('ili9488')
        self.payload('ili9341')
        self.payload('ili9488', folder='.')
        for model in ('vga',):
            with self.subTest(model=model), self.assertRaisesRegex(RuntimeError, 'VGA'):
                app.validated_payload('en', model)

    def test_vga_requires_correct_video_tag_and_lcd_fallback(self):
        self.payload('ili9341', folder='vga')
        with self.assertRaisesRegex(RuntimeError, 'VGA'):
            app.validated_payload('en', 'vga')

    def test_vga_requires_ili9341_lcd_recovery(self):
        for replacement in (b'FRESLEST_LCD:ILI9488', b''):
            with self.subTest(replacement=replacement):
                directory = self.payload('vga')
                self.retag(directory, b'FRESLEST_LCD:ILI9341', replacement)
                with self.assertRaisesRegex(RuntimeError, 'VGA'):
                    app.validated_payload('en', 'vga')

    def test_vga_rejects_wrong_clock_tag(self):
        directory = self.payload('vga')
        self.retag(directory, b'FRESLEST_VIDEO:VGA25175_25175000', b'FRESLEST_VIDEO:WRONG_CLOCK')
        with self.assertRaisesRegex(RuntimeError, 'VGA'):
            app.validated_payload('en', 'vga')

    def test_lcd_rejects_vga_binary_even_with_matching_lcd_tag(self):
        for model in ('ili9488', 'ili9341'):
            for video in ('vga',):
                with self.subTest(model=model, video=video):
                    directory = self.payload(video, folder=model)
                    self.retag(directory, b'FRESLEST_LCD:ILI9341',
                               ('FRESLEST_LCD:' + model.upper()).encode())
                    with self.assertRaisesRegex(RuntimeError, model.upper()):
                        app.validated_payload('en', model)

    def test_legacy_lcd_rejects_vga_tag(self):
        self.payload('vga', folder='.')
        with self.assertRaisesRegex(RuntimeError, 'ILI9488'):
            app.validated_payload('en', 'ili9488')

    def retag(self, directory, old, new):
        manifest = json.loads((directory / 'SHA256.json').read_text())
        for name in ('freslest_os_s3.bin', 'freslest_doom_s3.bin'):
            data = (directory / name).read_bytes().replace(old, new)
            (directory / name).write_bytes(data)
            manifest['firmware/' + name] = hashlib.sha256(data).hexdigest()
        (directory / 'SHA256.json').write_text(json.dumps(manifest))

    def test_worker_flashes_the_selected_display(self):
        for model in ('ili9488', 'ili9341', 'vga'):
            self.payload(model)
        for model in ('ili9488', 'ili9341', 'vga'):
            with self.subTest(model=model):
                worker = app.Installer.__new__(app.Installer)
                worker.events = queue.Queue()
                worker.language_code = 'en'
                fake = mock.Mock()
                with mock.patch.dict(sys.modules, {'esptool': fake}):
                    worker.worker('COM7', 115200, model)
                fake.main.assert_called_once()
                args = fake.main.call_args.args[0]
                for address, name, _ in app.ITEMS:
                    self.assertIn(str(self.root / model / name), args)
                    self.assertEqual(args[args.index(str(self.root / model / name)) - 1], hex(address))
                self.assertEqual(list(worker.events.queue)[-1][0:2], ('done', True))

    def test_each_missing_vga_profile_prevents_serial_flash(self):
        self.payload('ili9341')
        for model in ('vga',):
            with self.subTest(model=model):
                worker = app.Installer.__new__(app.Installer)
                worker.events = queue.Queue()
                worker.language_code = 'en'
                fake = mock.Mock()
                with mock.patch.dict(sys.modules, {'esptool': fake}):
                    worker.worker('COM7', 115200, model)
                fake.main.assert_not_called()
                self.assertEqual(list(worker.events.queue)[-1][0:2], ('done', False))

    def test_missing_profile_prevents_any_serial_flash(self):
        worker = app.Installer.__new__(app.Installer)
        worker.events = queue.Queue()
        worker.language_code = 'en'
        fake = mock.Mock()
        with mock.patch.dict(sys.modules, {'esptool': fake}):
            worker.worker('COM7', 115200, 'ili9341')
        fake.main.assert_not_called()
        self.assertFalse(list(worker.events.queue)[-1][1])


if __name__ == '__main__':
    unittest.main()
