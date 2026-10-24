"""Compatibility entry point for the standalone browser tests."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name('run_native_browser_tests.py')),run_name='__main__')
