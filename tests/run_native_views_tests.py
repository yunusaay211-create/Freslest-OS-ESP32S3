"""Actual Files and Browser UI/input on both canvas sizes; native text paging."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];a=r/'OS/freslest/src/freslest'
with tempfile.TemporaryDirectory() as d:
 for model in (9488,9341):
  common=['g++','-std=c++17','-g','-Wall','-Wextra','-Werror=misleading-indentation','-fsanitize=address,undefined','-fno-pie','-no-pie',f'-DFRESLEST_LCD_MODEL={model}','-I'+str(a)]
  if model==9341:common+=['-DFRESLEST_TEST_NATIVE_UI=1']
  exe=Path(d)/f'files{model}'
  subprocess.run(common+['-I'+str(r/'OS/tests/stubs'),str(r/'OS/tests/test_files_ui.cpp'),str(a/'file_browser.cpp'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  exe=Path(d)/f'browser{model}'
  subprocess.run(common+['-I'+str(r/'tests/browser_stubs'),str(a/'browser_html.cpp'),str(r/'tests/test_browser_native_ui.cpp'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
 # Native text pages must fit twelve rows without dropping bytes between pages.
 probe=Path(d)/'pages.cpp';probe.write_text(r'''
#include "file_browser.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
int main(){auto root=std::filesystem::temp_directory_path()/("native_pages_"+std::to_string(getpid()));std::filesystem::create_directory(root);
 std::string original;for(int i=0;i<2000;i++)original+=char('A'+i%26);std::ofstream(root/"long.txt")<<original;
 std::string mount=root.string(),restored;FileBrowser f(mount.c_str());assert(f.refresh()&&f.count()==1);bool more=true;unsigned page=0;
 while(more){char b[FileBrowser::TEXT_PAGE+1];assert(f.textPage(0,page++,b,sizeof(b),more));restored+=b;}
 assert(restored==original);assert(FileBrowser::TEXT_PAGE==552);std::filesystem::remove_all(root);}
''');exe=Path(d)/'pages'
 subprocess.run(['g++','-std=c++17','-DFRESLEST_LCD_MODEL=9341','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(a),str(a/'file_browser.cpp'),str(probe),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: actual Files/Browser native and legacy views, USB actions, pointer mapping and navigation')
