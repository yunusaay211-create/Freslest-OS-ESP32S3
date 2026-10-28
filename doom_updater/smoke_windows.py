from pathlib import Path
import ctypes,json,subprocess,time
from ctypes import wintypes
r=Path(__file__).resolve().parents[1];u=ctypes.WinDLL('user32',use_last_error=True)
u.IsWindowVisible.argtypes=[wintypes.HWND];u.GetWindowTextW.argtypes=[wintypes.HWND,wintypes.LPWSTR,ctypes.c_int]
u.PostMessageW.argtypes=[wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM]
CALLBACK=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM);u.EnumWindows.argtypes=[CALLBACK,wintypes.LPARAM]
p=subprocess.Popen([str(r/'dist/Freslest Doom VGA.exe')]);found=[]
@CALLBACK
def visit(hwnd,_):
 text=ctypes.create_unicode_buffer(512);u.GetWindowTextW(hwnd,text,512)
 if u.IsWindowVisible(hwnd) and text.value=='Freslest Doom VGA | 1.1':found.append(hwnd)
 return True
try:
 deadline=time.monotonic()+75
 while time.monotonic()<deadline and not found:
  u.EnumWindows(visit,0)
  if p.poll() is not None:raise RuntimeError('Updater closed before its window appeared')
  time.sleep(.2)
 assert found
 time.sleep(2);assert p.poll() is None
 u.PostMessageW(found[0],0x10,0,0);assert p.wait(timeout=15)==0
 (r/'dist/DOOM_WINDOWS_SMOKE.json').write_text(json.dumps({'native_windows_launch':True,'title':'Freslest Doom VGA | 1.1','flashing_started':False})+'\n')
finally:
 if p.poll() is None:subprocess.run(['taskkill','/PID',str(p.pid),'/T','/F'],check=False)
