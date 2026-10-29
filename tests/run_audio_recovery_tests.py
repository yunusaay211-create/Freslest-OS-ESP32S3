"""Exercise production DFPlayer state transitions with real packet framing."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CASES = ('lost_insert', 'sd_query', 'latest_volume', 'sleep', 'silent_rx', 'init_volume', 'runtime_volume')

def main():
    app = ROOT / 'OS/freslest/src/freslest'
    base = ROOT / 'OS/tests'
    with tempfile.TemporaryDirectory() as folder:
        exe = Path(folder) / 'audio'
        subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g',
                        '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                        '-I'+str(base/'stubs'), '-I'+str(app), '-I'+str(ROOT/'OS/components/nofrendo/src'),
                        str(app/'audio_manager.cpp'), str(app/'nes_audio.cpp'),
                        str(base/'mock_audio.cpp'), str(base/'test_mp3_recovery.cpp'), '-o', str(exe)], check=True)
        results = [subprocess.run([str(exe), case], env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0'}).returncode for case in CASES]
        if any(results):
            raise SystemExit(1)

if __name__ == '__main__':
    main()
