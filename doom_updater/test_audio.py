"""Run Doom's real UART music state machine without requiring a WAD in CI."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(sys.platform == 'win32', 'Native C checks run in the firmware job')
class DoomAudio(unittest.TestCase):
    def test_uart_music_state_machine(self):
        doom = ROOT / 'Doom'
        includes = [doom / 'tests/stubs', doom / 'main',
                    doom / 'components/prboom/include',
                    doom / 'components/prboom-esp32-compat/include']
        with tempfile.TemporaryDirectory() as folder:
            exe = Path(folder) / 'audio'
            subprocess.run([
                'gcc', '-std=gnu17', '-Wall', '-Wextra', '-Werror',
                '-Wno-misleading-indentation', '-g',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-fno-pie', '-no-pie',
                *('-I' + str(path) for path in includes),
                str(doom / 'tests/test_audio.c'), '-o', str(exe),
            ], check=True)
            subprocess.run([str(exe), '--music-only'], check=True,
                           env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})


if __name__ == '__main__':
    unittest.main()
