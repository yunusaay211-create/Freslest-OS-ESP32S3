// SPDX-License-Identifier: MIT
// LCD RGB565 framebuffer -> RGB332 VGA. Timing/pins from supplied V1.0 core.
#include "freslest_video.h"
#include "vga_pixels.h"
#include <string.h>
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
static freslest_display_t output;
static bool boot_read,started,first_frame_ready;
static esp_lcd_panel_handle_t panel;
static uint8_t *fb;
static volatile uint32_t vsyncs,last_vsync_us;
static uint32_t presentations;
static int64_t started_us;
bool freslest_video_save_output(freslest_display_t value){
 if(value!=FRESLEST_DISPLAY_LCD&&value!=FRESLEST_DISPLAY_VGA)return false;
 nvs_handle_t n;esp_err_t e=nvs_open("fl_video",NVS_READWRITE,&n);
 if(e==ESP_OK){e=nvs_set_u8(n,"output_v2",value);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
 return e==ESP_OK;
}
freslest_display_t freslest_video_boot_output(void){
 if(boot_read)return output;
 boot_read=true;uint8_t value=0;nvs_handle_t n;
 if(nvs_open("fl_video",NVS_READONLY,&n)==ESP_OK){nvs_get_u8(n,"output_v2",&value);nvs_close(n);}
 gpio_config_t c={.pin_bit_mask=1ULL<<16,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
 gpio_config(&c);vTaskDelay(pdMS_TO_TICKS(5));
 if(!gpio_get_level(16)){value=0;freslest_video_save_output(FRESLEST_DISPLAY_LCD);}
 output=value==FRESLEST_DISPLAY_VGA?FRESLEST_DISPLAY_VGA:FRESLEST_DISPLAY_LCD;return output;
}
freslest_display_t freslest_video_output(void){return output;}
static bool IRAM_ATTR on_vsync(esp_lcd_panel_handle_t p,const esp_lcd_rgb_panel_event_data_t *event,void *ctx){
 (void)p;(void)event;(void)ctx;vsyncs++;last_vsync_us=(uint32_t)esp_timer_get_time();return false;
}
bool freslest_video_begin(void){
 if(freslest_video_boot_output()!=FRESLEST_DISPLAY_VGA)return true;
 if(started)return true;
 // Keep SPI LCD deselected; never attach LCD backlight LEDC to shared GPIO41.
 gpio_config_t bl={.pin_bit_mask=(1ULL<<41)|(1ULL<<10),.mode=GPIO_MODE_OUTPUT};
 gpio_config(&bl);gpio_set_level(41,0);gpio_set_level(10,1);
 esp_lcd_rgb_panel_config_t c={0};c.clk_src=LCD_CLK_SRC_DEFAULT;c.data_width=8;c.bits_per_pixel=8;c.num_fbs=1;
 c.hsync_gpio_num=47;c.vsync_gpio_num=48;c.pclk_gpio_num=-1;c.de_gpio_num=-1;c.disp_gpio_num=-1;
 const int pins[]={41,8,13,14,2,42,40,39};for(int i=0;i<8;i++)c.data_gpio_nums[i]=pins[i];
 c.timings.pclk_hz=25175000;c.timings.h_res=VGA_WIDTH;c.timings.v_res=VGA_HEIGHT;
 c.timings.hsync_pulse_width=96;c.timings.hsync_back_porch=48;c.timings.hsync_front_porch=16;
 c.timings.vsync_pulse_width=2;c.timings.vsync_back_porch=33;c.timings.vsync_front_porch=10;
 c.timings.flags.hsync_idle_low=0;c.timings.flags.vsync_idle_low=0;c.flags.fb_in_psram=1;
 esp_lcd_rgb_panel_event_callbacks_t callbacks={.on_vsync=on_vsync};
 if(esp_lcd_new_rgb_panel(&c,&panel)!=ESP_OK)goto fail;
 if(esp_lcd_rgb_panel_get_frame_buffer(panel,1,(void**)&fb)!=ESP_OK||!fb)goto fail;
 memset(fb,0,VGA_WIDTH*VGA_HEIGHT);
 if(esp_lcd_rgb_panel_register_event_callbacks(panel,&callbacks,NULL)!=ESP_OK)goto fail;
 if(esp_lcd_panel_reset(panel)!=ESP_OK||esp_lcd_panel_init(panel)!=ESP_OK)goto fail;
 started_us=esp_timer_get_time();first_frame_ready=false;started=true;
 ESP_LOGW("video","VGA V1.0 RGB332 640x480, target PCLK 25175000 Hz, PSRAM; LCD RGB565 source");return true;
fail:
 if(panel){esp_lcd_panel_del(panel);panel=NULL;}fb=NULL;
 ESP_LOGE("video","VGA init failed. Hold BACK during reset for LCD.");return false;
}
bool freslest_video_healthy(void){return output==FRESLEST_DISPLAY_LCD||
 (started&&presentations&&vsyncs>=3&&(uint32_t)((uint32_t)esp_timer_get_time()-last_vsync_us)<500000);}
bool freslest_video_starting(void){
 if(output!=FRESLEST_DISPLAY_VGA||!started||first_frame_ready)return false;
 if(freslest_video_healthy()){first_frame_ready=true;return false;}
 return esp_timer_get_time()-started_us<10000000;
}
static void submit(const void *src,int w,int h,const uint32_t *pal,bool swap,bool cursor,int x,int y){
 if(!started||!vga_convert(fb,src,w,h,pal,swap,cursor,x,y))return;
 if(esp_cache_msync(fb,VGA_WIDTH*VGA_HEIGHT,ESP_CACHE_MSYNC_FLAG_DIR_C2M)==ESP_OK)presentations++;
}
void freslest_video_rgb565(const uint16_t *s,int w,int h,bool swap,bool cursor,int x,int y){submit(s,w,h,NULL,swap,cursor,x,y);}
void freslest_video_indexed(const uint8_t *s,int w,int h,const uint32_t *pal,bool cursor,int x,int y){if(pal)submit(s,w,h,pal,false,cursor,x,y);}
void freslest_video_diagnostics(void){
 ESP_LOGW("video","output=%s RGB332 started=%u syncs=%lu images=%lu sync-age=%lu us healthy=%u; D0..7=41,8,13,14,2,42,40,39 H=47 V=48",
 output==FRESLEST_DISPLAY_LCD?"LCD":"VGA",started,(unsigned long)vsyncs,(unsigned long)presentations,
 (unsigned long)((uint32_t)esp_timer_get_time()-last_vsync_us),freslest_video_healthy());
}
