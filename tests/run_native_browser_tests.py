from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];app=r/'OS/freslest/src/freslest'
with tempfile.TemporaryDirectory() as directory:
 exe=Path(directory)/'html'
 subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(app),str(app/'browser_html.cpp'),str(r/'tests/test_browser_html.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 exe=Path(directory)/'ui'
 subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(app),'-I'+str(r/'tests/browser_stubs'),str(app/'browser_html.cpp'),str(r/'tests/test_browser_native_ui.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 exe=Path(directory)/'http'
 subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(app),'-I'+str(r/'tests/browser_stubs'),'-I'+str(r/'shared_components/freslest_hid/include'),str(app/'browser_html.cpp'),str(r/'tests/test_browser_native.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
