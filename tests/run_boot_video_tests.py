"""Exercise the real Doom frame health call with the real composite backend.
The asynchronous first frame must not be reported as a failed OTA trial.
"""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'Doom/components/prboom-esp32-compat/i_video.c').read_text()
a=s.index('void I_FinishUpdate (void)');b=s.index('\nint16_t lcdpal',a)
frame=s[a:b]
pre=r'''
#include "video_stubs/host_video.h"
#include "../shared_components/freslest_video/freslest_video.c"
#include <stdio.h>
static struct {void *data;} screens[1];
static int health_ok,health_failed,draws;
static bool freslest_ota_health_tick(bool healthy){if(healthy)health_ok++;else health_failed++;return healthy;}
static void spi_lcd_send(uint16_t *screen){
 static uint32_t pal[256];pal[1]=255;draws++;
 freslest_video_indexed((const uint8_t*)screen,320,240,pal,false,0,0);
}
'''
test=r'''
int main(void){
 uint8_t pixels[320*240];memset(pixels,1,sizeof(pixels));screens[0].data=pixels;
 stored=1;assert(freslest_video_begin());
 I_FinishUpdate();assert(draws==1&&health_ok==0&&health_failed==0);
 for(int i=0;i<2;i++){host_time+=17000;host_av_field(engine);I_FinishUpdate();}
 assert(!health_ok&&!health_failed); // sync alone is not an accepted frame
 render_pending();host_time+=17000;host_av_field(engine);I_FinishUpdate();
 assert(health_ok==1&&!health_failed);
 host_time+=500001;I_FinishUpdate();assert(health_ok==1&&health_failed==1); // no new grace period after success
 first_frame_ready=false;presentations=vsyncs=0;engine->ownership.swaps=0;engine->stats.fields=0;started_us=host_time;
 I_FinishUpdate();assert(health_failed==1);
 host_time+=10000000;I_FinishUpdate();assert(health_failed==2); // missing output still reaches rollback
 freslest_av_close(engine);engine=NULL;vSemaphoreDelete(sync_sem);sync_sem=NULL;
 for(int i=0;i<3;i++)heap_caps_free(frames[i]);started=false;output=FRESLEST_DISPLAY_LCD;
 I_FinishUpdate();assert(health_ok==2); // synchronous LCD behavior preserved
 assert(!live&&!live_panels&&!live_sem);
 puts("PASS: actual Doom I_FinishUpdate and real composite backend defer first asynchronous frame, require presentation and three syncs, bound warmup, detect later failure immediately and preserve LCD health");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'boot.c';p.write_text(pre+frame+test);exe=p.with_suffix('')
 subprocess.run(['gcc','-std=gnu17','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-I'+str(r/'tests'),'-I'+str(r/'tests/video_stubs'),'-I'+str(r/'shared_components/freslest_video/include'),'-I'+str(r/'shared_components/freslest_av/include'),str(p),str(r/'shared_components/freslest_video/video_pixels.c'),str(r/'shared_components/freslest_video/video_slots.c'),str(r/'shared_components/freslest_av/av_signal.c'),str(r/'shared_components/freslest_av/av_ownership.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
