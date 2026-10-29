from pathlib import Path
import os,subprocess,tempfile,unittest,sys
R=Path(__file__).resolve().parents[1]
@unittest.skipIf(sys.platform=='win32','Native C checks run in the Doom firmware job')
class Runtime(unittest.TestCase):
 def test_busy_demo_always_releases_cpu_for_idle_watchdog(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)
   host=r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define pdMS_TO_TICKS(x) (x)
static int64_t now;
static unsigned sleeps;
static int64_t esp_timer_get_time(void){return now;}
static void vTaskDelay(int ticks){assert(ticks>0);++sleeps;now+=ticks*1000;}
static void* heap_caps_malloc(size_t n,int flags){assert(n==76800&&flags==3);return malloc(n);}
unsigned freslest_video_fit(void){return 90;}
bool doom_vga_try_present(const uint8_t*p,bool c,int x,int y){assert(p);(void)c;(void)x;(void)y;return true;}
'''
   (p/'host.h').write_text(host)
   for name in ('esp_heap_caps.h','esp_timer.h','freertos/FreeRTOS.h','freertos/task.h'):
    f=p/name;f.parent.mkdir(exist_ok=True);f.write_text('#include "host.h"\n')
   f=p/'busy.c'
   f.write_text('#include "host.h"\n#include "'+str(R/'Doom/components/prboom-esp32-compat/doom_vga_frame.c')+'"\n'+r'''
int main(void){
 static uint8_t pixels[76800];
 /* Slow demo rendering misses the 33ms submission deadline continuously.
  * A priority-5 producer must still block so the core's idle task can run. */
 for(unsigned frame=0;frame<1000;++frame){
  now+=40000;pixels[0]=(uint8_t)frame;
  doom_vga_send_frame(pixels,false,0,0);
  assert(sleeps==frame+1);
 }
 free(previous);
}
''')
   self.compile_run(f,p,[])
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
   f.write_text('''#define DOOM_RGB332_WORDS 1\n#include "vga_host.h"\n#include "'''+str(R/'Doom/components/freslest_video/doom_video.c')+'''"\n'''+r'''
int main(void){
 (void)nvs_set_u8;(void)nvs_commit;stored_v2=0;stored_preset25175=0;stored_fit=95;
 assert(freslest_video_boot_output()==FRESLEST_DISPLAY_VGA);
 assert(stored_v2==0&&stored_preset25175==0&&stored_fit==95);
 assert(freslest_video_fit()==95&&freslest_video_save_fit(80)&&stored_fit==95);
 boot_read=false;back=0;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD&&stored_v2==0);
 back=1;boot_read=false;assert(freslest_video_begin());
 assert(saved_config.timings.h_res==640&&saved_config.timings.v_res==480&&saved_config.timings.pclk_hz==25175000);
 assert(saved_config.data_width==16&&saved_config.bits_per_pixel==16&&saved_config.bounce_buffer_size_px==640*20);
 const int expected[]={41,8,13,14,2,42,40,39};
 for(unsigned i=0;i<8;++i)assert(saved_config.data_gpio_nums[i]==expected[i]);
 for(unsigned i=8;i<16;++i)assert(saved_config.data_gpio_nums[i]==-1);
 static uint8_t palette[768],pixels[76800];
 for(unsigned i=0;i<256;++i){palette[i*3]=i&224;palette[i*3+1]=(i&28)<<3;palette[i*3+2]=(i&3)<<6;}
 for(unsigned i=0;i<sizeof(pixels);++i)pixels[i]=(uint8_t)i;
 freslest_video_doom_palette(palette);assert(doom_vga_try_present(pixels,false,0,0));host_frame();
 vga_viewport_t view=vga_viewport(95);
 for(int y=0;y<480;++y)for(int x=0;x<640;++x){
  unsigned want=x<view.x||x>=view.x+view.w||y<view.y||y>=view.y+view.h?0:pixels[(y-view.y)*240/view.h*320+(x-view.x)*320/view.w];
  assert(host_active[y*640+x]==want);
 }
 back=0;assert(freslest_video_color_test());host_frame();
 for(unsigned bit=0;bit<8;++bit)assert(host_active[336*640+32+bit*72]==(1u<<bit));
 static uint8_t ui[480*320];uint32_t ui_palette[256]={0xffffff};
 freslest_video_indexed(ui,480,320,ui_palette,false,0,0);host_frame();
 assert(host_active[240*640+320]==255&&host_active[0]==0);
 assert(stored_v2==0&&stored_preset25175==0&&stored_fit==95);
}
''')
   self.compile_run(f,R/'tests/vga_stubs',[R/'shared_components/freslest_video/vga_pixels.c',R/'Doom/components/freslest_video/doom_rgb332.c'])
 def compile_run(self,f,headers,extra):
  exe=f.with_suffix('')
  subprocess.run(['gcc','-std=c17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-DFRESLEST_VGA_PRESET_ID=25175','-I'+str(headers),'-I'+str(R/'Doom/components/freslest_video/include'),'-I'+str(R/'shared_components/freslest_video/include'),str(f),*map(str,extra),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
if __name__=='__main__':unittest.main()
