"""Run the installer and preserved Viewer tests on a desktop session."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
commands = [[sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-p', 'test_*.py', '-v']]
commands += [[sys.executable, str(test)] for test in sorted((ROOT / 'viewer' / 'tests').glob('test_*.py'))]
for command in commands:
    subprocess.run(command, cwd=ROOT, check=True)
print('All installer and Viewer tests passed.')
