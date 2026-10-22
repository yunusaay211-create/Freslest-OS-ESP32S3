"""Verify the regression tests detect the original installer defects."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
baseline = root / '.baseline'
baseline.mkdir(exist_ok=True)
shutil.copyfile(root / 'tests/fixtures/installer_original.py', baseline / 'installer.py')
try:
    env = dict(os.environ, FRESLEST_TEST_INSTALLER_DIR=str(baseline))
    result = subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-p', 'test_installer.py', '-v'],
                            cwd=root, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout)
    if result.returncode == 0 or 'FAILED (failures=3)' not in result.stdout:
        raise RuntimeError('Expected three original installer regression failures.')
    print('Expected baseline failures confirmed: completion page, retry, stderr/newline logging.')
finally:
    shutil.rmtree(baseline)
