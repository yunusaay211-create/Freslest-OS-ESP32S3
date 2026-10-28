from pathlib import Path
import os,subprocess,tempfile,unittest,sys
R=Path(__file__).resolve().parents[1]
@unittest.skipIf(sys.platform=='win32','Native C checks run in the Doom firmware job')
class Runtime(unittest.TestCase):
 def test_doom_static_frames_palette_cursor_and_dropped_retry(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)
   host='''#include <stdbool.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n#include <assert.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\n#define pdMS_TO_TICKS(x) (x)\nstatic int64_t now;static int calls;static unsigned fit=90;static bool accept=true;\nstatic void* heap_caps_malloc(size_t n,int c){assert(n==76800&&c==3);return malloc(n);}\nstatic int64_t esp_timer_get_time(void){return now;}\nstatic void vTaskDelay(int n){now+=n*1000;}\nunsigned freslest_video_fit(void){return fit;}\nbool doom_vga_try_present(const uint8_t*p,bool c,int x,int y){assert(p);(void)c;(void)x;(void)y;calls++;return accept;}\n'''
   (p/'host.h').write_text('#pragma once\n'+host)
   for n in ['esp_heap_caps.h','esp_timer.h','freertos/FreeRTOS.h','freertos/task.h']:
    f=p/n;f.parent.mkdir(exist_ok=True);f.write_text('#include "host.h"\n')
   test='''#include "host.h"\n#include "'''+str(R/'Doom/components/prboom-esp32-compat/doom_vga_frame.c')+'''"\nint main(void){static uint8_t f[76800];for(int i=0;i<20;i++)doom_vga_send_frame(f,false,0,0);assert(calls==1&&now>=19*33000);f[0]=1;doom_vga_send_frame(f,false,0,0);assert(calls==2);doom_vga_palette_changed();doom_vga_send_frame(f,false,0,0);assert(calls==3);doom_vga_send_frame(f,true,2,3);doom_vga_send_frame(f,true,2,3);assert(calls==4);doom_vga_send_frame(f,true,3,3);assert(calls==5);doom_vga_send_frame(f,false,3,3);assert(calls==6);fit=95;doom_vga_send_frame(f,false,3,3);assert(calls==7);accept=false;f[9]=4;doom_vga_send_frame(f,false,0,0);assert(calls==8);accept=true;doom_vga_send_frame(f,false,0,0);assert(calls==9);doom_vga_send_frame(f,false,0,0);assert(calls==9);free(previous);}\n'''
   f=p/'test.c';f.write_text(test)
   self.compile_run(f,p,[])
 def test_doom_backend_does_not_write_os_nvs(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);f=p/'test.c'
   f.write_text('''#include "vga_host.h"\n#include "'''+str(R/'Doom/components/freslest_video/doom_video.c')+'''"\nint main(void){(void)nvs_set_u8;(void)nvs_commit;stored_v2=0;stored_preset25175=0;stored_fit=95;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_VGA);assert(stored_v2==0&&stored_preset25175==0&&stored_fit==95);assert(freslest_video_fit()==95);assert(freslest_video_save_fit(80));assert(stored_fit==95);boot_read=false;back=0;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD&&stored_v2==0);back=1;boot_read=false;assert(freslest_video_begin());assert(saved_config.timings.h_res==640&&saved_config.timings.v_res==480&&saved_config.timings.pclk_hz==25175000&&saved_config.data_width==8);}\n''')
   self.compile_run(f,R/'tests/vga_stubs',[R/'shared_components/freslest_video/vga_pixels.c',R/'shared_components/freslest_video/vga_doom.c'])
 def compile_run(self,f,headers,extra):
  exe=f.with_suffix('')
  subprocess.run(['gcc','-std=c17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-DFRESLEST_VGA_PRESET_ID=25175','-I'+str(headers),'-I'+str(R/'Doom/components/freslest_video/include'),'-I'+str(R/'shared_components/freslest_video/include'),str(f),*map(str,extra),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
if __name__=='__main__':unittest.main()
