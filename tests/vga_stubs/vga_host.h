#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
typedef int esp_err_t,nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define IRAM_ATTR
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_ENABLE 1
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define LCD_CLK_SRC_DEFAULT 0
#define ESP_CACHE_MSYNC_FLAG_DIR_C2M 1
#define pdMS_TO_TICKS(x) (x)
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en;} gpio_config_t;
typedef void* esp_lcd_panel_handle_t;
typedef struct {int dummy;} esp_lcd_rgb_panel_event_data_t;
typedef struct {bool (*on_vsync)(esp_lcd_panel_handle_t,const esp_lcd_rgb_panel_event_data_t*,void*);bool (*on_frame_buf_complete)(esp_lcd_panel_handle_t,const esp_lcd_rgb_panel_event_data_t*,void*);} esp_lcd_rgb_panel_event_callbacks_t;
typedef struct {int clk_src,data_width,bits_per_pixel,num_fbs;size_t bounce_buffer_size_px;int hsync_gpio_num,vsync_gpio_num,pclk_gpio_num,de_gpio_num,disp_gpio_num;int data_gpio_nums[16];struct {int pclk_hz,h_res,v_res,hsync_pulse_width,hsync_back_porch,hsync_front_porch,vsync_pulse_width,vsync_back_porch,vsync_front_porch;struct {int hsync_idle_low,vsync_idle_low;}flags;}timings;struct {int fb_in_psram;}flags;} esp_lcd_rgb_panel_config_t;
static int stored_fit=90;
static int stored_preset25175;
static int pending_fit,pending_v2,pending_preset25175;
static int stored_v2,legacy_output=1,back=1,right=1,nvs_failure,driver_failure,panels,creates,stage;
#ifdef DOOM_RGB332_WORDS
typedef uint16_t host_pixel_t;
#else
typedef uint8_t host_pixel_t;
#endif
static uint64_t pins;static int64_t now_us;static host_pixel_t host_fb[640*480],host_back[640*480],host_snapshot[640*480];
static host_pixel_t *host_active=host_fb,*host_selected=host_fb;static bool host_events=true;static unsigned host_draws;
static esp_lcd_rgb_panel_config_t saved_config;static esp_lcd_rgb_panel_event_callbacks_t callbacks;
static esp_err_t nvs_open(const char*n,int m,nvs_handle_t*h){
 assert(!strcmp(n,"fl_video"));(void)m;*h=1;
 pending_fit=stored_fit;pending_v2=stored_v2;pending_preset25175=stored_preset25175;
 return nvs_failure==1?-1:0;
}
static esp_err_t nvs_get_u8(nvs_handle_t h,const char*k,uint8_t*v){
 (void)h;
 if(!strcmp(k,"preset25175")){if(!stored_preset25175)return ESP_FAIL;*v=stored_preset25175;}
 else *v=!strcmp(k,"fit_pct")?stored_fit:!strcmp(k,"output_v2")?stored_v2:legacy_output;
 return ESP_OK;
}
static esp_err_t nvs_set_u8(nvs_handle_t h,const char*k,uint8_t v){
 (void)h;if(nvs_failure==2)return ESP_FAIL;
 if(!strcmp(k,"fit_pct"))pending_fit=v;
 else if(!strcmp(k,"output_v2"))pending_v2=v;
 else {assert(!strcmp(k,"preset25175"));if(nvs_failure==4)return ESP_FAIL;pending_preset25175=v;}
 return ESP_OK;
}
static esp_err_t nvs_commit(nvs_handle_t h){
 (void)h;if(nvs_failure==3)return ESP_FAIL;
 stored_fit=pending_fit;stored_v2=pending_v2;stored_preset25175=pending_preset25175;return ESP_OK;
}
static void nvs_close(nvs_handle_t h){(void)h;}
static int gpio_config(const gpio_config_t*c){pins|=c->pin_bit_mask;return 0;}
static int gpio_get_level(int p){assert(p==16||p==7);return p==16?back:right;}
static int gpio_set_level(int p,int v){assert((p==41&&v==0)||(p==10&&v==1));return 0;}
static void host_frame(void){
 host_active=host_selected;memcpy(host_snapshot,host_active,sizeof(host_snapshot));
 if(callbacks.on_frame_buf_complete)callbacks.on_frame_buf_complete((void*)1,NULL,NULL);
 if(callbacks.on_vsync)callbacks.on_vsync((void*)1,NULL,NULL);
}
static void vTaskDelay(int ms){now_us+=ms*1000;if(host_events&&panels&&callbacks.on_vsync)host_frame();}
static int64_t esp_timer_get_time(void){return now_us;}
static void host_log(const char*t,const char*f,...){assert(t&&f);}
#define ESP_LOGE(...) host_log(__VA_ARGS__)
#define ESP_LOGW(...) host_log(__VA_ARGS__)
static int esp_lcd_new_rgb_panel(const esp_lcd_rgb_panel_config_t*c,esp_lcd_panel_handle_t*p){creates++;saved_config=*c;if(driver_failure==1)return -1;*p=(void*)1;panels++;stage=1;return 0;}
static int esp_lcd_rgb_panel_get_frame_buffer(esp_lcd_panel_handle_t p,int n,void**a,void**b){assert(p&&n==2&&stage==1);if(driver_failure==2)return -1;*a=host_fb;*b=host_back;stage=2;return 0;}
static int esp_lcd_rgb_panel_register_event_callbacks(esp_lcd_panel_handle_t p,const esp_lcd_rgb_panel_event_callbacks_t*c,void*ctx){assert(p&&!ctx);callbacks=*c;return driver_failure==3?-1:0;}
static int esp_lcd_panel_reset(esp_lcd_panel_handle_t p){assert(p&&stage==2);for(unsigned i=0;i<sizeof(host_fb)/sizeof(host_fb[0]);i++)assert(!host_fb[i]&&!host_back[i]);return driver_failure==4?-1:0;}
static int esp_lcd_panel_init(esp_lcd_panel_handle_t p){assert(p);return driver_failure==5?-1:0;}
static int esp_lcd_panel_del(esp_lcd_panel_handle_t p){assert(p);panels--;return 0;}
static int esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t p,int x,int y,int w,int h,const void* pixels){
 assert(p&&x==0&&y==0&&w==640&&h==480);
 assert(pixels!=host_active); // CPU must only prepare an unowned back buffer.
 assert(!memcmp(host_active,host_snapshot,sizeof(host_snapshot)));
 ++host_draws;if(driver_failure==6)return -1;
 host_selected=(host_pixel_t*)pixels;return 0;
}
