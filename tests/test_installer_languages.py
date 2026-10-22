import contextlib
import io
import queue
import sys
import tkinter as tk
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'installer'))
import installer as app


def widgets(widget):
    for child in widget.winfo_children():
        yield child
        yield from widgets(child)


def labels(widget):
    result = []
    for child in widgets(widget):
        try:
            result.append(str(child.cget('text')))
        except tk.TclError:
            pass
    return result


class WorkerLanguageTests(unittest.TestCase):
    def worker(self):
        worker = app.Installer.__new__(app.Installer)
        worker.events = queue.Queue()
        worker.language_code = 'en'
        return worker

    def test_english_flash_log_and_success_message(self):
        worker = self.worker()
        fake = mock.Mock(main=lambda args: print('esptool output'))
        with mock.patch.dict(sys.modules, {'esptool': fake}), contextlib.redirect_stdout(io.StringIO()):
            worker.worker('COM7')
        events = list(worker.events.queue)
        self.assertIn(('done', True, 'Installation completed. The board has restarted.'), events)
        log = ''.join(value for value in events if isinstance(value, str))
        self.assertIn('Verified: bootloader.bin\n', log)
        self.assertIn('Connecting: COM7\n', log)

    def test_english_invalid_payload_message_prevents_flashing(self):
        worker = self.worker()
        fake = mock.Mock()
        with mock.patch.dict(sys.modules, {'esptool': fake}), mock.patch.object(app.hashlib, 'sha256', return_value=mock.Mock(hexdigest=lambda: 'invalid')):
            worker.worker('COM7')
        fake.main.assert_not_called()
        result = [value for value in worker.events.queue if isinstance(value, tuple)][-1]
        self.assertEqual(result, ('done', False, 'Invalid or missing file: bootloader.bin'))


class WizardLanguageTests(unittest.TestCase):
    def setUp(self):
        self.window = app.Installer()
        self.window.update_idletasks()

    def tearDown(self):
        for token in self.window.tk.splitlist(self.window.tk.call('after', 'info')):
            self.window.after_cancel(token)
        self.window.destroy()

    def choose(self, language):
        picker = next((widget for widget in widgets(self.window)
                       if isinstance(widget, app.ttk.Combobox)
                       and set(widget.cget('values')) == {'Türkçe', 'English'}), None)
        self.assertIsNotNone(picker, 'Welcome screen has no Turkish/English selector')
        picker.set(language)
        picker.event_generate('<<ComboboxSelected>>')
        self.window.update_idletasks()

    def test_language_switch_preserves_com_and_speed_across_back_navigation(self):
        self.choose('English')
        self.assertIn('Continue  →', labels(self.window))
        self.window.ports()
        self.assertIn('2 / 3   SELECT COM PORT', labels(self.window))
        self.window.port.set('COM27')
        self.window.baud.set('115200')
        back = next(widget for widget in widgets(self.window)
                    if isinstance(widget, app.ttk.Button) and str(widget.cget('text')) == '← Back')
        back.invoke()
        self.choose('Türkçe')
        self.window.ports()
        self.assertIn('2 / 3   COM PORTUNU SEÇ', labels(self.window))
        self.assertEqual(self.window.port.get(), 'COM27')
        self.assertEqual(self.window.baud.get(), '115200')

    def test_english_validation_and_overwrite_confirmation(self):
        self.choose('English')
        self.window.ports()
        self.window.port.set('invalid')
        with mock.patch.object(app.messagebox, 'showerror') as error:
            self.window.confirm()
        self.assertEqual(error.call_args.args[0], 'COM port')
        self.assertIn('valid Windows COM port', error.call_args.args[1])
        self.window.port.set('COM7')
        with mock.patch.object(app.messagebox, 'askyesno', return_value=False) as question:
            self.window.confirm()
        self.assertEqual(question.call_args.args[0], 'Confirm installation')
        self.assertIn('OS/Doom/WAD/LittleFS will be overwritten', question.call_args.args[1])

    def finish(self, success):
        with mock.patch.object(app.threading.Thread, 'start'):
            self.window.flash('COM7')
        self.window.events.put(('done', success, 'Device details'))
        self.window.poll()

    def test_english_completion_screen(self):
        self.choose('English')
        self.finish(True)
        self.assertIn('INSTALLATION COMPLETE', labels(self.window))
        self.assertIn('Close', labels(self.window))
        self.assertIn('Save log', labels(self.window))
        self.assertNotIn('KURULUM TAMAMLANDI', labels(self.window))

    def test_english_failure_retries_in_english(self):
        self.choose('English')
        self.finish(False)
        self.assertIn('INSTALLATION FAILED', labels(self.window))
        retry = next(widget for widget in widgets(self.window)
                     if isinstance(widget, app.ttk.Button) and str(widget.cget('text')) == 'Try again')
        retry.invoke()
        self.assertIn('2 / 3   SELECT COM PORT', labels(self.window))


if __name__ == '__main__':
    unittest.main()
