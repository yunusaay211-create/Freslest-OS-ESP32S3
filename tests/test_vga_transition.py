"""VGA transport and conversion regression, run in the Linux firmware job."""
from pathlib import Path
import os,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
@unittest.skipIf(sys.platform=='win32','C sanitizer tests run in Linux firmware job')
class VgaTransition(unittest.TestCase):
 def test_bounce_flash_cache_config(self):
  for project in ('OS','Doom'):
   for name in ('sdkconfig','sdkconfig.defaults'):
    config=(ROOT/project/name).read_text()
    self.assertNotIn('CONFIG_LCD_RGB_RESTART_IN_VSYNC=y',config)
    self.assertIn('CONFIG_LCD_RGB_ISR_IRAM_SAFE=y',config)
    for key in ('SPIRAM_XIP_FROM_PSRAM','SPIRAM_FETCH_INSTRUCTIONS','SPIRAM_RODATA'):
     self.assertIn('CONFIG_'+key+'=y',config)

 def test_converter_stack_budget(self):
  with tempfile.TemporaryDirectory() as d:
   obj=Path(d)/'pixels.o'
   subprocess.run(['gcc','-std=gnu17','-O2','-fstack-usage','-I'+str(ROOT/'shared_components/freslest_video/include'),'-c',str(ROOT/'shared_components/freslest_video/vga_pixels.c'),'-o',str(obj)],check=True)
   line=next(x for x in obj.with_suffix('.su').read_text().splitlines() if 'vga_convert_fit' in x)
   self.assertLessEqual(int(line.split('\t')[1]),256,'VGA converter exhausts the OS main-task stack')
   subprocess.run(['gcc','-std=gnu17','-O2','-fstack-usage','-I'+str(ROOT/'shared_components/freslest_video/include'),'-c',str(ROOT/'shared_components/freslest_video/vga_doom.c'),'-o',str(obj)],check=True)
   line=next(x for x in obj.with_suffix('.su').read_text().splitlines() if 'vga_convert_doom' in x)
   self.assertLessEqual(int(line.split('\t')[1]),256,'Doom VGA converter exhausts its task stack')

 def test_vga_presets(self):
  for preset,clock in ((25175,25175000),):
   with self.subTest(preset=preset), tempfile.TemporaryDirectory() as d:
    exe=Path(d)/'preset'
    subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',f'-DFRESLEST_VGA_PRESET_ID={preset}',f'-DFRESLEST_VGA_PCLK_HZ={clock}','-I'+str(ROOT/'tests/vga_stubs'),'-I'+str(ROOT/'shared_components/freslest_video/include'),str(ROOT/'tests/test_vga_preset.c'),str(ROOT/'shared_components/freslest_video/vga_pixels.c'),str(ROOT/'shared_components/freslest_video/vga_doom.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

 def test_actual_vga_backend(self):
  self.assertTrue((ROOT/'shared_components/freslest_video/vga_pixels.c').is_file(), 'VGA converter is missing; composite backend still active')
  with tempfile.TemporaryDirectory() as d:
   exe=Path(d)/'vga'
   subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(ROOT/'tests/vga_stubs'),'-I'+str(ROOT/'shared_components/freslest_video/include'),str(ROOT/'tests/test_vga_backend.c'),str(ROOT/'shared_components/freslest_video/vga_pixels.c'),str(ROOT/'shared_components/freslest_video/vga_doom.c'),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
if __name__=='__main__':unittest.main()
