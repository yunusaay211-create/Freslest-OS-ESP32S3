#include <stdio.h>
#include "vga_host.h"
#include "../shared_components/freslest_video/freslest_video.c"
static void reset(void){
 assert(!panels);boot_read=started=first_frame_ready=false;output=FRESLEST_DISPLAY_LCD;stored_v2=0;back=1;vsyncs=presentations=0;now_us=last_vsync_us=0;driver_failure=nvs_failure=0;
}
int main(void){
 reset();assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD);assert(!creates); // legacy output=1 is NOT VGA
 boot_read=false;stored_v2=1;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD);
 boot_read=false;stored_v2=2;back=0;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD&&stored_v2==0);back=1;
 assert(!freslest_video_save_output((freslest_display_t)1));assert(!freslest_video_save_output((freslest_display_t)99));
 for(int fail=1;fail<=3;fail++){nvs_failure=fail;assert(!freslest_video_save_output(FRESLEST_DISPLAY_VGA));}nvs_failure=0;
 for(int fail=1;fail<=5;fail++){reset();stored_v2=2;driver_failure=fail;assert(!freslest_video_begin());assert(!panels&&!started&&!fb);}
 reset();stored_v2=2;memset(host_fb,0xa5,sizeof(host_fb));assert(freslest_video_begin()&&panels==1);assert(pins&(1ULL<<41));
 const int pins_expected[]={41,8,13,14,2,42,40,39};assert(!memcmp(saved_config.data_gpio_nums,pins_expected,sizeof(pins_expected)));
 assert(saved_config.data_width==8&&saved_config.bits_per_pixel==8&&saved_config.flags.fb_in_psram&&saved_config.num_fbs==1);
 assert(saved_config.hsync_gpio_num==47&&saved_config.vsync_gpio_num==48&&saved_config.pclk_gpio_num==-1&&saved_config.de_gpio_num==-1);
 assert(saved_config.timings.pclk_hz==25175000&&saved_config.timings.h_res==640&&saved_config.timings.v_res==480);
 assert(saved_config.timings.hsync_pulse_width==96&&saved_config.timings.hsync_back_porch==48&&saved_config.timings.hsync_front_porch==16);
 assert(saved_config.timings.vsync_pulse_width==2&&saved_config.timings.vsync_back_porch==33&&saved_config.timings.vsync_front_porch==10);
 assert(!saved_config.timings.flags.hsync_idle_low&&!saved_config.timings.flags.vsync_idle_low);
 assert(freslest_video_starting()&&!freslest_video_healthy());
 uint16_t raw[]={0xf800,0x07e0,0x001f,0xffff},swap[]={0x00f8,0xe007,0x1f00,0xffff};
 freslest_video_rgb565(raw,2,2,false,false,0,0);assert(host_fb[0]==0xe0&&host_fb[639]==0x1c&&host_fb[479*640]==3&&host_fb[640*480-1]==255);
 freslest_video_rgb565(swap,2,2,true,false,0,0);assert(host_fb[0]==0xe0&&host_fb[639]==0x1c&&host_fb[479*640]==3);
 uint8_t indexed[]={1,2,3,4};uint32_t pal[256]={0};pal[1]=0x0000ff;pal[2]=0x00ff00;pal[3]=0xff0000;pal[4]=0xffffff;
 freslest_video_indexed(indexed,2,2,pal,false,0,0);assert(host_fb[0]==0xe0&&host_fb[639]==0x1c&&host_fb[479*640]==3&&host_fb[640*480-1]==255);
 uint16_t frame[320*240];for(unsigned i=0;i<320*240;i++)frame[i]=0xffff;
 freslest_video_rgb565(frame,320,240,false,true,0,0);assert(!host_fb[0]&&frame[0]==0xffff&&host_fb[639]==255);
 unsigned count=presentations;freslest_video_rgb565(NULL,1,1,false,false,0,0);freslest_video_rgb565(raw,481,2,false,false,0,0);assert(presentations==count);
 for(int i=0;i<3;i++){now_us+=17000;callbacks.on_vsync(panel,NULL,NULL);}assert(freslest_video_healthy()&&!freslest_video_starting());
 now_us+=500001;assert(!freslest_video_healthy()&&!freslest_video_starting());
 first_frame_ready=false;started_us=now_us;assert(freslest_video_starting());now_us+=10000000;assert(!freslest_video_starting());
 esp_lcd_panel_del(panel);panel=NULL;fb=NULL;started=false;assert(!panels);
 puts("PASS: actual VGA RGB332 config/timing/pins, PSRAM, NVS migration/BACK recovery, setup failure cleanup, RGB565 endian/indexed/cursor conversion, bounded startup and stalled VSYNC health");
}
