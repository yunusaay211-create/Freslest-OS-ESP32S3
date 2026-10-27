#include <stdio.h>
#include "vga_host.h"
#include "../shared_components/freslest_video/freslest_video.c"
static void reset(void){
 assert(!panels);boot_read=started=first_frame_ready=false;output=FRESLEST_DISPLAY_LCD;stored_v2=0;back=1;vsyncs=presentations=0;now_us=last_vsync_us=0;frame_events=last_frame_us=0;pending=false;front=0;host_active=host_selected=host_fb;host_events=true;memset(host_snapshot,0,sizeof(host_snapshot));memset(&callbacks,0,sizeof(callbacks));driver_failure=nvs_failure=0;
}
int main(void){
 reset();assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD);assert(!creates); // legacy output=1 is NOT VGA
 boot_read=false;stored_v2=1;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD);
 boot_read=false;stored_v2=2;back=0;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD&&stored_v2==0);back=1;
 assert(!freslest_video_save_output((freslest_display_t)1));assert(!freslest_video_save_output((freslest_display_t)99));
 for(int fail=1;fail<=3;fail++){nvs_failure=fail;assert(!freslest_video_save_output(FRESLEST_DISPLAY_VGA));}nvs_failure=0;
 for(int fail=1;fail<=5;fail++){reset();stored_v2=2;driver_failure=fail;assert(!freslest_video_begin());assert(!panels&&!started&&!fb[0]&&!fb[1]);}
 reset();stored_v2=2;memset(host_fb,0xa5,sizeof(host_fb));assert(freslest_video_begin()&&panels==1);assert(pins&(1ULL<<41));
 const int pins_expected[]={41,8,13,14,2,42,40,39};assert(!memcmp(saved_config.data_gpio_nums,pins_expected,sizeof(pins_expected)));
 assert(saved_config.data_width==8&&saved_config.bits_per_pixel==8&&saved_config.flags.fb_in_psram&&saved_config.num_fbs==2&&saved_config.bounce_buffer_size_px>=640*10);
 assert(saved_config.hsync_gpio_num==47&&saved_config.vsync_gpio_num==48&&saved_config.pclk_gpio_num==-1&&saved_config.de_gpio_num==-1);
 assert(saved_config.timings.pclk_hz==25175000&&saved_config.timings.h_res==640&&saved_config.timings.v_res==480);
 assert(saved_config.timings.hsync_pulse_width==96&&saved_config.timings.hsync_back_porch==48&&saved_config.timings.hsync_front_porch==16);
 assert(saved_config.timings.vsync_pulse_width==2&&saved_config.timings.vsync_back_porch==33&&saved_config.timings.vsync_front_porch==10);
 assert(!saved_config.timings.flags.hsync_idle_low&&!saved_config.timings.flags.vsync_idle_low);
 assert(freslest_video_starting()&&!freslest_video_healthy());
 uint16_t white=0xffff;freslest_video_rgb565(&white,1,1,false,false,0,0);
 assert(host_selected[0]==0&&host_selected[24*640+32]==255&&host_selected[456*640+608]==0);
 host_frame();
 assert(!freslest_video_save_fit(79)&&!freslest_video_save_fit(101)&&!freslest_video_save_fit(91));
 nvs_failure=3;assert(!freslest_video_save_fit(100)&&freslest_video_fit()==90);nvs_failure=0;
 assert(freslest_video_save_fit(100));
 uint16_t raw[]={0xf800,0x07e0,0x001f,0xffff},swap[]={0x00f8,0xe007,0x1f00,0xffff};
 freslest_video_rgb565(raw,2,2,false,false,0,0);
 // Presenting must not mutate the buffer currently being scanned by DMA.
 assert(!memcmp(host_active,host_snapshot,sizeof(host_snapshot)));
 host_frame();
 assert(host_selected[0]==0xe0&&host_selected[639]==0x1c&&host_selected[479*640]==3&&host_selected[640*480-1]==255);
 freslest_video_rgb565(swap,2,2,true,false,0,0);assert(host_selected[0]==0xe0&&host_selected[639]==0x1c&&host_selected[479*640]==3);
 uint8_t indexed[]={1,2,3,4};uint32_t pal[256]={0};pal[1]=0x0000ff;pal[2]=0x00ff00;pal[3]=0xff0000;pal[4]=0xffffff;
 freslest_video_indexed(indexed,2,2,pal,false,0,0);assert(host_selected[0]==0xe0&&host_selected[639]==0x1c&&host_selected[479*640]==3&&host_selected[640*480-1]==255);
 // Uniform palette colors must remain solid. The former Bayer pattern
 // introduced colored dots even into a completely flat source image.
 const uint8_t grayIndex[]={5};
 const unsigned levels[]={32,64,128,192,255};
 const uint8_t codes[]={0x24,0x49,0x92,0xdb,0xff};
 for(unsigned n=0;n<5;n++){
  unsigned level=levels[n];pal[5]=level|(level<<8)|(level<<16);
  freslest_video_indexed(grayIndex,1,1,pal,false,0,0);
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)assert(host_selected[y*640+x]==codes[n]);
 }
 pal[5]=0x0000ff;freslest_video_indexed(grayIndex,1,1,pal,false,0,0);assert(host_selected[0]==0xe0);
 pal[5]=0x00ff00;freslest_video_indexed(grayIndex,1,1,pal,false,0,0);assert(host_selected[0]==0x1c);
 // Dedicated Doom protocol uses raw R,G,B triplets, not swapped RGB565.
 static uint8_t doom[320*240],rgb[768];
 rgb[3]=255;rgb[7]=255;rgb[11]=255;rgb[12]=rgb[13]=rgb[14]=255;
 for(int y=0;y<240;y++)for(int x=0;x<320;x++)doom[y*320+x]=1+(x>=160)+2*(y>=120);
 freslest_video_doom_palette(rgb);freslest_video_doom(doom,false,0,0);
 assert(host_selected[0]==0xe0&&host_selected[639]==0x1c&&host_selected[479*640]==3&&host_selected[640*480-1]==255);
 memset(doom,5,sizeof(doom));
 for(unsigned n=0;n<5;n++){
  rgb[15]=rgb[16]=rgb[17]=levels[n];freslest_video_doom_palette(rgb);freslest_video_doom(doom,false,0,0);
  for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)assert(host_selected[y*640+x]==codes[n]);
 }
 // A genuine red damage palette stays red, then a later green palette replaces it.
 rgb[15]=255;rgb[16]=rgb[17]=0;freslest_video_doom_palette(rgb);freslest_video_doom(doom,false,0,0);assert(host_selected[0]==0xe0);
 rgb[15]=0;rgb[16]=255;freslest_video_doom_palette(rgb);freslest_video_doom(doom,false,0,0);assert(host_selected[0]==0x1c);
 for(unsigned fit=80;fit<=100;fit+=5){
  assert(freslest_video_save_fit(fit));memset(doom,4,sizeof(doom));freslest_video_doom(doom,false,0,0);
  unsigned left=(100-fit)*640/200,top=(100-fit)*480/200;
  assert(host_selected[top*640+left]==255&&host_selected[(479-top)*640+639-left]==255);
  if(left)assert(host_selected[0]==0&&host_selected[top*640+left-1]==0&&host_selected[(480-top)*640]==0);
 }
 uint16_t frame[320*240];for(unsigned i=0;i<320*240;i++)frame[i]=0xffff;
 freslest_video_rgb565(frame,320,240,false,true,0,0);assert(!host_selected[0]&&frame[0]==0xffff&&host_selected[639]==255);
 // Pending frames are immutable if completion stops; timeout drops the new
 // image instead of overwriting a frame the DMA may still own.
 unsigned draws=host_draws;host_events=false;uint8_t snapshot[640*480];memcpy(snapshot,host_selected,sizeof(snapshot));
 freslest_video_rgb565(raw,2,2,false,false,0,0);assert(host_draws==draws&&!memcmp(snapshot,host_selected,sizeof(snapshot)));
 host_events=true;host_frame();
 driver_failure=6;freslest_video_rgb565(raw,2,2,false,false,0,0);assert(host_draws==draws+1);driver_failure=0;
 freslest_video_rgb565(raw,2,2,false,false,0,0);host_frame();
 unsigned count=presentations;freslest_video_rgb565(NULL,1,1,false,false,0,0);freslest_video_rgb565(raw,481,2,false,false,0,0);assert(presentations==count);
 for(int i=0;i<3;i++){now_us+=17000;host_frame();}assert(freslest_video_healthy()&&!freslest_video_starting());
 now_us+=500001;assert(!freslest_video_healthy()&&!freslest_video_starting());
 first_frame_ready=false;started_us=now_us;assert(freslest_video_starting());now_us+=10000000;assert(!freslest_video_starting());
 esp_lcd_panel_del(panel);panel=NULL;fb[0]=fb[1]=NULL;started=false;assert(!panels);
 puts("PASS: actual VGA RGB332 config/timing/pins, PSRAM, NVS migration/BACK recovery, setup failure cleanup, RGB565 endian/indexed/cursor conversion, bounded startup and stalled VSYNC health");
}
