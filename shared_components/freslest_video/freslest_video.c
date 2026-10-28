// SPDX-License-Identifier: MIT
// LCD RGB565 framebuffer -> RGB332 VGA. Timing/pins from supplied V1.0 core.
#include "freslest_video.h"
#include "vga_pixels.h"
#include <string.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#ifdef ESP_PLATFORM
#include "hal/lcd_ll.h"
#include "soc/lcd_cam_struct.h"
#endif
#ifndef FRESLEST_VGA_PCLK_HZ
#define FRESLEST_VGA_PCLK_HZ 25175000
#endif
#ifndef FRESLEST_VGA_PRESET_ID
#define FRESLEST_VGA_PRESET_ID 0
#endif
#if FRESLEST_VGA_PRESET_ID == 25175
#define VGA_PRESET_KEY "preset25175"
#define VGA_PRESET_MARKER "FRESLEST_VIDEO:VGA25175_25175000"
#elif FRESLEST_VGA_PRESET_ID != 0
#error Unsupported VGA preset
#endif
static freslest_display_t output;
static bool boot_read,started,first_frame_ready;
static esp_lcd_panel_handle_t panel;
static uint8_t *fb[2];
static unsigned front;
static bool pending;
static uint32_t queued_event;
static volatile uint32_t frame_events,last_frame_us;
static uint32_t dropped;
static unsigned fit_percent=90;
static volatile uint32_t vsyncs,last_vsync_us;
static uint32_t presentations;
static int64_t started_us;
bool freslest_video_save_output(freslest_display_t value){
 if(value!=FRESLEST_DISPLAY_LCD&&value!=FRESLEST_DISPLAY_VGA)return false;
 nvs_handle_t n;esp_err_t e=nvs_open("fl_video",NVS_READWRITE,&n);
 if(e==ESP_OK){e=nvs_set_u8(n,"output_v2",value);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
 return e==ESP_OK;
}
#if FRESLEST_VGA_PRESET_ID != 0
static bool save_preset_output(freslest_display_t value){
 nvs_handle_t n;esp_err_t e=nvs_open("fl_video",NVS_READWRITE,&n);
 if(e==ESP_OK){
  e=nvs_set_u8(n,"output_v2",value);
  if(e==ESP_OK)e=nvs_set_u8(n,VGA_PRESET_KEY,1);
  if(e==ESP_OK)e=nvs_commit(n);
  nvs_close(n);
 }
 return e==ESP_OK;
}
#endif
freslest_display_t freslest_video_boot_output(void){
 if(boot_read)return output;
 boot_read=true;uint8_t value=0;nvs_handle_t n;
#if FRESLEST_VGA_PRESET_ID != 0
 bool preset_seen=false;
 puts(VGA_PRESET_MARKER);
#endif
 if(nvs_open("fl_video",NVS_READONLY,&n)==ESP_OK){nvs_get_u8(n,"output_v2",&value);
  uint8_t fit=90;if(nvs_get_u8(n,"fit_pct",&fit)==ESP_OK&&fit>=80&&fit<=100&&fit%5==0)fit_percent=fit;
#if FRESLEST_VGA_PRESET_ID != 0
  uint8_t seen=0;preset_seen=nvs_get_u8(n,VGA_PRESET_KEY,&seen)==ESP_OK&&seen==1;
#endif
  nvs_close(n);}
 gpio_config_t c={.pin_bit_mask=1ULL<<16,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
 gpio_config(&c);vTaskDelay(pdMS_TO_TICKS(5));
 bool recovery=!gpio_get_level(16);
#if FRESLEST_VGA_PRESET_ID != 0
 if(!preset_seen){
  // Consume each source-built preset once. BACK has priority on its first boot too.
  value=recovery?FRESLEST_DISPLAY_LCD:FRESLEST_DISPLAY_VGA;
  if(!save_preset_output((freslest_display_t)value))
   ESP_LOGW("video","VGA preset save failed; BACK still forces LCD recovery");
 }else
#endif
 if(recovery){value=FRESLEST_DISPLAY_LCD;
  if(!freslest_video_save_output(FRESLEST_DISPLAY_LCD))
   ESP_LOGW("video","LCD recovery output was not persisted");
 }
 output=value==FRESLEST_DISPLAY_VGA?FRESLEST_DISPLAY_VGA:FRESLEST_DISPLAY_LCD;return output;
}
freslest_display_t freslest_video_output(void){return output;}
unsigned freslest_video_fit(void){return fit_percent;}
bool freslest_video_save_fit(unsigned percent){
 if(percent<80||percent>100||percent%5)return false;
 nvs_handle_t n;esp_err_t e=nvs_open("fl_video",NVS_READWRITE,&n);
 if(e==ESP_OK){e=nvs_set_u8(n,"fit_pct",percent);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
 if(e==ESP_OK)fit_percent=percent;
 return e==ESP_OK;
}
static bool IRAM_ATTR on_vsync(esp_lcd_panel_handle_t p,const esp_lcd_rgb_panel_event_data_t *event,void *ctx){
 (void)p;(void)event;(void)ctx;vsyncs++;last_vsync_us=(uint32_t)esp_timer_get_time();return false;
}
static bool IRAM_ATTR on_frame(esp_lcd_panel_handle_t p,const esp_lcd_rgb_panel_event_data_t *event,void *ctx){
 (void)p;(void)event;(void)ctx;frame_events++;last_frame_us=(uint32_t)esp_timer_get_time();return false;
}
bool freslest_video_begin(void){
 if(freslest_video_boot_output()!=FRESLEST_DISPLAY_VGA)return true;
 if(started)return true;
 // Keep SPI LCD deselected; never attach LCD backlight LEDC to shared GPIO41.
 gpio_config_t bl={.pin_bit_mask=(1ULL<<41)|(1ULL<<10),.mode=GPIO_MODE_OUTPUT};
 gpio_config(&bl);gpio_set_level(41,0);gpio_set_level(10,1);
 esp_lcd_rgb_panel_config_t c={0};c.clk_src=LCD_CLK_SRC_DEFAULT;c.data_width=8;c.bits_per_pixel=8;c.num_fbs=2;c.bounce_buffer_size_px=VGA_WIDTH*20;
 c.hsync_gpio_num=47;c.vsync_gpio_num=48;c.pclk_gpio_num=-1;c.de_gpio_num=-1;c.disp_gpio_num=-1;
 const int pins[]={41,8,13,14,2,42,40,39};for(int i=0;i<8;i++)c.data_gpio_nums[i]=pins[i];
 c.timings.pclk_hz=FRESLEST_VGA_PCLK_HZ;c.timings.h_res=VGA_WIDTH;c.timings.v_res=VGA_HEIGHT;
 c.timings.hsync_pulse_width=96;c.timings.hsync_back_porch=48;c.timings.hsync_front_porch=16;
 c.timings.vsync_pulse_width=2;c.timings.vsync_back_porch=33;c.timings.vsync_front_porch=10;
 c.timings.flags.hsync_idle_low=0;c.timings.flags.vsync_idle_low=0;c.flags.fb_in_psram=1;
 esp_lcd_rgb_panel_event_callbacks_t callbacks={.on_vsync=on_vsync,.on_frame_buf_complete=on_frame};
 if(esp_lcd_new_rgb_panel(&c,&panel)!=ESP_OK)goto fail;
 if(esp_lcd_rgb_panel_get_frame_buffer(panel,2,(void**)&fb[0],(void**)&fb[1])!=ESP_OK||!fb[0]||!fb[1])goto fail;
 memset(fb[0],0,VGA_WIDTH*VGA_HEIGHT);memset(fb[1],0,VGA_WIDTH*VGA_HEIGHT);front=0;pending=false;
 if(esp_lcd_rgb_panel_register_event_callbacks(panel,&callbacks,NULL)!=ESP_OK)goto fail;
 if(esp_lcd_panel_reset(panel)!=ESP_OK)goto fail;
#ifdef ESP_PLATFORM
 // Select a literal RGB332 byte stream, independent of a previous LCD mode.
 lcd_ll_enable_rgb_yuv_convert(&LCD_CAM,false);
 lcd_ll_reverse_dma_data_bit_order(&LCD_CAM,false);
 lcd_ll_swap_dma_data_byte_order(&LCD_CAM,false);
 lcd_ll_enable_swizzle(&LCD_CAM,false);
#endif
 if(esp_lcd_panel_init(panel)!=ESP_OK)goto fail;
 started_us=esp_timer_get_time();first_frame_ready=false;started=true;
 puts("FRESLEST_VGA:RAW332_C4_READY");
 // The same diagnostic is available in OS and Doom before any app palette.
 gpio_config_t keys={.pin_bit_mask=1ULL<<7,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
 gpio_config(&keys);
 if(!gpio_get_level(7))freslest_video_color_test();
 ESP_LOGW("video","VGA V1.0 RGB332 640x480 59.94 Hz, target PCLK %u Hz, native 8-bit RGB332; double PSRAM frames + 20-line bounce",(unsigned)FRESLEST_VGA_PCLK_HZ);return true;
fail:
 if(panel){esp_lcd_panel_del(panel);panel=NULL;}fb[0]=fb[1]=NULL;pending=false;
 ESP_LOGE("video","VGA init failed. Hold BACK during reset for LCD.");return false;
}
bool freslest_video_healthy(void){return output==FRESLEST_DISPLAY_LCD||
 (started&&presentations&&vsyncs>=3&&frame_events>=3&&(uint32_t)((uint32_t)esp_timer_get_time()-last_frame_us)<500000&&(uint32_t)((uint32_t)esp_timer_get_time()-last_vsync_us)<500000);}
bool freslest_video_starting(void){
 if(output!=FRESLEST_DISPLAY_VGA||!started||first_frame_ready)return false;
 if(freslest_video_healthy()){first_frame_ready=true;return false;}
 return esp_timer_get_time()-started_us<10000000;
}
// on_frame fires after the driver has copied a complete frame to internal
// bounce buffers and selected its next source. Only then may we reuse the old
// PSRAM frame. A missing IRQ must never turn a timeout into buffer reuse.
static uint8_t *acquire_frame(void){
 if(!started)return NULL;
 if(pending){
  int64_t deadline=esp_timer_get_time()+100000;
  while(frame_events==queued_event){
   if(esp_timer_get_time()>=deadline){dropped++;return NULL;}
   vTaskDelay(pdMS_TO_TICKS(1));
  }
  front^=1;pending=false;
 }
 return fb[front^1];
}
static bool publish_frame(uint8_t *target){
 if(esp_lcd_panel_draw_bitmap(panel,0,0,VGA_WIDTH,VGA_HEIGHT,target)!=ESP_OK){dropped++;return false;}
 // Sample AFTER the driver's selection. If an IRQ races this read, waiting
 // one extra frame is safe; sampling before draw could release too early.
 queued_event=frame_events;pending=true;presentations++;return true;
}
static void raw_test_pattern(uint8_t *target){
 memset(target,0,VGA_WIDTH*VGA_HEIGHT);
 // Literal bus bytes deliberately bypass all color packing and palettes.
 static const uint8_t bars[]={0xff,0xfc,0x1f,0x1c,0xe3,0xe0,0x03,0x00};
 for(int y=64;y<160;y++)for(int x=32;x<608;x++)target[y*VGA_WIDTH+x]=bars[(x-32)/72];
 for(int row=0;row<4;row++)for(int y=176+row*40;y<208+row*40;y++)for(int x=32;x<608;x++){
  unsigned step=(x-32)/72;
  target[y*VGA_WIDTH+x]=row==0?step<<5:row==1?step<<2:row==2?step>>1:(step<<5)|(step<<2)|(step>>1);
 }
 for(int y=336;y<392;y++)for(int x=32;x<608;x++)target[y*VGA_WIDTH+x]=(uint8_t)(1u<<((x-32)/72));
 // A small C4 identifier remains independent of the application's fonts.
 static const uint8_t glyphs[2][5]={{0x3e,0x41,0x41,0x41,0x22},{0x18,0x14,0x12,0x7f,0x10}};
 for(int g=0;g<2;g++)for(int x=0;x<5;x++)for(int y=0;y<7;y++)if(glyphs[g][x]&(1u<<y))
  for(int dy=0;dy<3;dy++)for(int dx=0;dx<3;dx++)target[(32+y*3+dy)*VGA_WIDTH+32+g*18+x*3+dx]=255;
}
bool freslest_video_color_test(void){
 if(output!=FRESLEST_DISPLAY_VGA||!started)return false;
 uint8_t *target=acquire_frame();if(!target)return false;
 raw_test_pattern(target);if(!publish_frame(target))return false;
 puts("FRESLEST_VGA:RAW332_C4 white yellow cyan green magenta red blue black; ramps R G B gray; bits D0..D7");
 int64_t deadline=esp_timer_get_time()+10000000;
 while(esp_timer_get_time()<deadline&&gpio_get_level(16))vTaskDelay(pdMS_TO_TICKS(20));
 return true;
}
static bool submit(const void *src,int w,int h,const uint32_t *pal,bool swap,bool cursor,int x,int y){
 if(!src||w<1||h<1||w>480||h>360)return false;
 uint8_t *target=acquire_frame();
 return target&&vga_convert_fit(target,src,w,h,pal,swap,cursor,x,y,fit_percent)&&publish_frame(target);
}
bool freslest_video_rgb565(const uint16_t *s,int w,int h,bool swap,bool cursor,int x,int y){return submit(s,w,h,NULL,swap,cursor,x,y);}
void freslest_video_indexed(const uint8_t *s,int w,int h,const uint32_t *pal,bool cursor,int x,int y){if(pal)submit(s,w,h,pal,false,cursor,x,y);}
void freslest_video_doom_palette(const uint8_t *rgb888){if(rgb888)vga_doom_palette(rgb888);}
void freslest_video_doom(const uint8_t *indices,bool cursor,int x,int y){
 if(!indices)return;
 uint8_t *target=acquire_frame();
 if(target&&vga_convert_doom(target,indices,cursor,x,y,fit_percent))publish_frame(target);
}
void freslest_video_diagnostics(void){
 ESP_LOGW("video","output=%s RGB332 PCLK=25.175MHz 640x480 59.94Hz started=%u syncs=%lu images=%lu sync-age=%lu us healthy=%u dropped=%lu frames=%lu fit=%u%%; D0..7=41,8,13,14,2,42,40,39 H=47 V=48",
 output==FRESLEST_DISPLAY_LCD?"LCD":"VGA",started,(unsigned long)vsyncs,(unsigned long)presentations,
 (unsigned long)((uint32_t)esp_timer_get_time()-last_vsync_us),freslest_video_healthy(),(unsigned long)dropped,(unsigned long)frame_events,fit_percent);
}
