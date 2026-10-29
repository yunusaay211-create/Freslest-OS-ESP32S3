from pathlib import Path
import queue
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'installer'))
import installer as app

class SdWizardTests(unittest.TestCase):
    def setUp(self):
        self.window=app.Installer()
        self.addCleanup(self.cleanup)
    def cleanup(self):
        for token in self.window.tk.splitlist(self.window.tk.call('after','info')):
            self.window.after_cancel(token)
        if hasattr(self.window,'progress') and self.window.progress.winfo_exists():self.window.progress.stop()
        self.window.destroy()
    def labels(self):
        def visit(widget):
            for child in widget.winfo_children():
                try:yield str(child.cget('text'))
                except app.tk.TclError:pass
                yield from visit(child)
        return list(visit(self.window))
    def test_sd_page_needs_no_com_and_has_separate_result(self):
        self.window.sd_page()
        self.assertEqual(self.window.port.get(),'')
        self.assertIn('SD KART SESLERİ',self.labels())
        with tempfile.TemporaryDirectory() as folder:
            self.window.sd_target.set(folder)
            with mock.patch.object(app,'validate_card_volume',return_value=Path(folder)),mock.patch.object(app.messagebox,'askyesno',return_value=True),mock.patch.object(app.threading.Thread,'start'):
                self.window.confirm_sd()
            self.assertTrue(self.window.busy)
            self.window.events.put(('sd_progress',112,112,'MP3/0112.mp3'))
            self.window.events.put(('sd_done',True,'112 ses doğrulandı.'))
            self.window.poll()
        self.assertFalse(self.window.busy)
        self.assertIn('SD SESLERİ HAZIR',self.labels())
        self.assertNotIn('KURULUM TAMAMLANDI',self.labels())
    def test_cancel_does_not_start_copy(self):
        with tempfile.TemporaryDirectory() as folder:
            self.window.sd_target.set(folder)
            with mock.patch.object(app,'validate_card_volume',return_value=Path(folder)),mock.patch.object(app.messagebox,'askyesno',return_value=False),mock.patch.object(app.threading.Thread,'start') as start:
                self.window.confirm_sd()
                start.assert_not_called()
        self.assertFalse(self.window.busy)
    def test_sd_worker_never_flashes_firmware(self):
        worker=app.Installer.__new__(app.Installer);worker.events=queue.Queue()
        fake=mock.Mock()
        with mock.patch.dict(sys.modules,{'esptool':fake}),mock.patch.object(app,'install_audio',return_value={'copied':112,'verified':112}) as install:
            worker.sd_worker(Path('card'))
        install.assert_called_once()
        fake.main.assert_not_called()
        self.assertEqual(list(worker.events.queue)[-1][0:2],('sd_done',True))

if __name__=='__main__':unittest.main()
