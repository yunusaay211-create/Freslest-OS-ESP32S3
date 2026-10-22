"""Launch the actual EXEs and verify their native Windows top-level windows."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
user32 = ctypes.WinDLL('user32', use_last_error=True)
user32.IsWindowVisible.argtypes = [wintypes.HWND]
user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
CALLBACK = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user32.EnumWindows.argtypes = [CALLBACK, wintypes.LPARAM]
checks = []
diagnostics_path = ROOT / 'dist/INSTALLER_DIAGNOSTICS.json'
subprocess.run([str(ROOT / 'dist/Freslest Installer.exe'), '--diagnostics', str(diagnostics_path)], check=True, timeout=60)
diagnostics = json.loads(diagnostics_path.read_text(encoding='utf-8'))
if not diagnostics['ok'] or not diagnostics['frozen'] or len(diagnostics['firmware']) != 7:
    raise RuntimeError('The actual frozen installer did not validate its bundled flash components.')
for name, title in [('Freslest Installer.exe', 'Freslest Installer | OS 1.0'),
                    ('Freslest USB Viewer.exe', 'Freslest USB Viewer | OS 1.0')]:
    exe = ROOT / 'dist' / name
    process = subprocess.Popen([str(exe)], cwd=ROOT / 'dist')
    found = []
    @CALLBACK
    def visit(hwnd, _):
        text = ctypes.create_unicode_buffer(512)
        user32.GetWindowTextW(hwnd, text, 512)
        if user32.IsWindowVisible(hwnd) and text.value == title:
            found.append(hwnd)
        return True
    try:
        deadline = time.monotonic() + 75
        while time.monotonic() < deadline:
            user32.EnumWindows(visit, 0)
            if found:
                break
            if process.poll() is not None:
                raise RuntimeError(f'{name} exited before opening a window: {process.returncode}')
            time.sleep(.2)
        if not found:
            raise RuntimeError(f'No visible window from {name}')
        # Keep the GUI alive long enough for Viewer auto-connect and timers.
        time.sleep(2)
        if process.poll() is not None:
            raise RuntimeError(f'{name} crashed after startup')
        checks.append({'file': name, 'visible_title': title, 'native_windows_launch': True})
        user32.PostMessageW(found[0], 0x0010, 0, 0)
        result = process.wait(timeout=15)
        if result != 0:
            raise RuntimeError(f'{name} did not close cleanly: {result}')
    finally:
        if process.poll() is None:
            subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], check=False)
(ROOT / 'dist' / 'WINDOWS_SMOKE_TEST.json').write_text(json.dumps(checks, indent=2), encoding='utf-8')
print(json.dumps(checks, indent=2))
