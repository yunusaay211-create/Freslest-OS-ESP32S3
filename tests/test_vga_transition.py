"""VGA transport and conversion regression, run in the Linux firmware job."""
from pathlib import Path
import os,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipIf(sys.platform=='win32','C sanitizer tests run in Linux firmware job')
class VgaTransition(unittest.TestCase):
 def test_actual_vga_backend(self):
  self.assertTrue((ROOT/'shared_components/freslest_video/vga_pixels.c').is_file(), 'VGA converter is missing; composite backend still active')
  with tempfile.TemporaryDirectory() as d:
   exe=Path(d)/'vga'
   subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(ROOT/'tests/vga_stubs'),'-I'+str(ROOT/'shared_components/freslest_video/include'),str(ROOT/'tests/test_vga_backend.c'),str(ROOT/'shared_components/freslest_video/vga_pixels.c'),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
if __name__=='__main__':unittest.main()
