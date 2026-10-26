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
            window.update_idletasks()
            self.assertIn('1 / 3   DISPLAYS', [str(w.cget('text')) for w in window.winfo_children() if isinstance(w, app.tk.Label)])
            choices = [w for w in window.winfo_children() if isinstance(w, app.tk.Radiobutton)]
            self.assertEqual(len(choices), 2)
            next(w for w in choices if w.cget('value') == 'ili9341').invoke()
            window.port.set('COM7')
            window.baud.set('115200')
            window.ports()
            window.displays()
            self.assertEqual(window.display.get(), 'ili9341')
            window.language_code = 'tr'
            window.wizard()
            self.assertEqual(window.display.get(), 'ili9341')
            with mock.patch.object(app.threading, 'Thread') as thread:
                window.flash('COM7')
            self.assertEqual(thread.call_args.kwargs['args'], ('COM7', 115200, 'ili9341'))
        finally:
            for token in window.tk.splitlist(window.tk.call('after', 'info')):
                window.after_cancel(token)
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
            data = (f'FRESLEST_LCD:{model.upper()}' if 's3.bin' in name else name).encode()
            (directory / name).write_bytes(data)
            manifest['firmware/' + name] = hashlib.sha256(data).hexdigest()
        (directory / 'SHA256.json').write_text(json.dumps(manifest))
        return directory

    def test_each_display_selects_its_own_verified_os_and_doom(self):
        for model in ('ili9488', 'ili9341'):
            self.payload(model)
        for model in ('ili9488', 'ili9341'):
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

    def test_worker_flashes_the_selected_display(self):
        self.payload('ili9488')
        self.payload('ili9341')
        worker = app.Installer.__new__(app.Installer)
        worker.events = queue.Queue()
        worker.language_code = 'en'
        fake = mock.Mock()
        with mock.patch.dict(sys.modules, {'esptool': fake}):
            worker.worker('COM7', 115200, 'ili9341')
        fake.main.assert_called_once()
        args = fake.main.call_args.args[0]
        self.assertIn(str(self.root / 'ili9341/freslest_os_s3.bin'), args)
        self.assertIn(str(self.root / 'ili9341/freslest_doom_s3.bin'), args)
        self.assertNotIn(str(self.root / 'ili9488/freslest_os_s3.bin'), args)

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
