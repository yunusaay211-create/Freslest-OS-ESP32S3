import contextlib
import io
import os
import queue
import sys
import tkinter as tk
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, os.environ.get('FRESLEST_TEST_INSTALLER_DIR', str(Path(__file__).resolve().parents[1] / 'installer')))
import installer as app


def texts(widget):
    result = []
    for child in widget.winfo_children():
        try:
            result.append(str(child.cget('text')))
        except tk.TclError:
            pass
        result.extend(texts(child))
    return result


class WizardTests(unittest.TestCase):
    def setUp(self):
        self.window = app.Installer()
        self.window.update_idletasks()

    def tearDown(self):
        for token in self.window.tk.splitlist(self.window.tk.call('after', 'info')):
            self.window.after_cancel(token)
        self.window.destroy()

    def finish(self, success, message):
        with mock.patch.object(app.threading.Thread, 'start'):
            self.window.flash('COM7')
        self.window.events.put(('done', success, message))
        with mock.patch.object(app.messagebox, 'showinfo'), mock.patch.object(app.messagebox, 'showerror'):
            self.window.poll()

    def test_success_opens_a_dedicated_completion_page(self):
        self.finish(True, 'Kart yeniden başlatıldı.')
        self.assertIn('KURULUM TAMAMLANDI', texts(self.window))
        self.assertNotIn('3 / 3   FIRMWARE YÜKLENİYOR', texts(self.window))

    def test_failure_offers_a_retry_without_restarting_the_exe(self):
        self.finish(False, 'COM7 kullanımda')
        retry = next((w for w in self.window.winfo_children()
                      if isinstance(w, app.ttk.Button) and str(w.cget('text')) == 'Tekrar dene'), None)
        self.assertIsNotNone(retry, 'Failure screen has no retry button')
        retry.invoke()
        self.assertIn('2 / 3   COM PORTUNU SEÇ', texts(self.window))


class FlashWorkerTests(unittest.TestCase):
    def test_esptool_stderr_is_visible_in_the_installer_log(self):
        worker = app.Installer.__new__(app.Installer)
        worker.events = queue.Queue()
        observed = []
        def flash(args):
            observed.extend(args)
            print('diagnostic from stderr', file=sys.stderr)
            print('flash output')
        fake = mock.Mock(main=flash)
        with mock.patch.dict(sys.modules, {'esptool': fake}), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            worker.worker('COM7')
        values = list(worker.events.queue)
        self.assertTrue(any(v == ('done', True, 'Yükleme tamamlandı. Kart yeniden başlatıldı.') for v in values))
        logs = ''.join(v for v in values if isinstance(v, str))
        self.assertIn('diagnostic from stderr', logs)
        self.assertIn('flash output\n', logs)
        self.assertIn('0x460000', observed)

    def test_corrupted_firmware_prevents_any_flash_write(self):
        worker = app.Installer.__new__(app.Installer)
        worker.events = queue.Queue()
        fake = mock.Mock()
        with mock.patch.dict(sys.modules, {'esptool': fake}), mock.patch.object(app.hashlib, 'sha256', return_value=mock.Mock(hexdigest=lambda: 'invalid')):
            worker.worker('COM7')
        fake.main.assert_not_called()
        result = [v for v in worker.events.queue if isinstance(v, tuple)][-1]
        self.assertFalse(result[1])


if __name__ == '__main__':
    unittest.main()
