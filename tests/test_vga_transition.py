"""VGA transport and conversion regression, run in the Linux firmware job."""
from pathlib import Path
import os,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipIf(sys.platform=='win32','C sanitizer tests run in Linux firmware job')
class VgaTransition(unittest.TestCase):
 def test_dma_realigns_at_vblank(self):
  for project in ('OS','Doom'):
   for name in ('sdkconfig','sdkconfig.defaults'):
    self.assertTrue('CONFIG_LCD_RGB_RESTART_IN_VSYNC=y' in (ROOT/project/name).read_text(),f'{project}/{name}: VBlank DMA recovery disabled')

 def test_converter_stack_budget(self):
  with tempfile.TemporaryDirectory() as d:
   obj=Path(d)/'pixels.o'
   subprocess.run(['gcc','-std=gnu17','-O2','-fstack-usage','-I'+str(ROOT/'shared_components/freslest_video/include'),'-c',str(ROOT/'shared_components/freslest_video/vga_pixels.c'),'-o',str(obj)],check=True)
   line=next(x for x in obj.with_suffix('.su').read_text().splitlines() if 'vga_convert' in x)
   self.assertLessEqual(int(line.split('\t')[1]),256,'VGA converter exhausts the OS main-task stack')

 def test_actual_vga_backend(self):
  self.assertTrue((ROOT/'shared_components/freslest_video/vga_pixels.c').is_file(), 'VGA converter is missing; composite backend still active')
  with tempfile.TemporaryDirectory() as d:
   exe=Path(d)/'vga'
   subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(ROOT/'tests/vga_stubs'),'-I'+str(ROOT/'shared_components/freslest_video/include'),str(ROOT/'tests/test_vga_backend.c'),str(ROOT/'shared_components/freslest_video/vga_pixels.c'),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
if __name__=='__main__':unittest.main()
