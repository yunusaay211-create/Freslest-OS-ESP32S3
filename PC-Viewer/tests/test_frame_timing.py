"""A frame must still be presented when the Windows clock tick is unchanged."""
import queue
from pathlib import Path
from types import SimpleNamespace
import sys
import threading
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import freslest_viewer as app


class FrameTimingTests(unittest.TestCase):
    def viewer(self):
        return SimpleNamespace(
            events=queue.Queue(), frame_lock=threading.Lock(), connected=True,
            pending_frame=(480, 320, b'latest frame', 7), image=mock.Mock(),
            _revision=0, last_frame_at=100.0, _status_at=0,
            connection_label='NATIVE test', show_image=mock.Mock(),
            status=mock.Mock(), root=mock.Mock(), pump=mock.Mock())

    def test_equal_clock_ticks_keep_latest_frame_without_crashing(self):
        viewer = self.viewer()
        with mock.patch.object(app.time, 'monotonic', return_value=100.0):
            app.Viewer.pump(viewer)
        viewer.image.configure.assert_called_once_with(
            width=480, height=320, data=b'latest frame', format='PPM')
        viewer.show_image.assert_called_once()
        self.assertIn('0.0 fps', viewer.status.set.call_args.args[0])
        viewer.root.after.assert_called_once()

    def test_elapsed_clock_ticks_report_measured_frame_rate(self):
        viewer = self.viewer()
        with mock.patch.object(app.time, 'monotonic', return_value=100.5):
            app.Viewer.pump(viewer)
        self.assertIn('2.0 fps', viewer.status.set.call_args.args[0])


if __name__ == '__main__':
    unittest.main()
