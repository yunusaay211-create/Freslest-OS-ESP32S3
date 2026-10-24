// ESP32-S3 LCD_CAM -> external 8-bit resistor ladder. One output per boot.
#include "freslest_video.h"
#include "video_pixels.h"
#include "video_slots.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "freslest_av.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"
static freslest_display_t output;
static bool boot_read,started;
static freslest_av_handle_t engine;
static av_renderer_t renderer;
static int prepared=-1;
static uint8_t *wave,*frames[3],phase[256][4];
static SemaphoreHandle_t sync_sem;
static video_slots_t slots;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t vsyncs,presentations,last_vsync_us;
static uint32_t unchanged_frames;
static int64_t started_us;
static bool first_frame_ready;
static freslest_tv_config_t tv={0,0,100};
static freslest_tv_color_t color={0,100,0};
static uint32_t color_revision=1,rendered_color_revision=1;
static freslest_tv_color_t color_clamp(freslest_tv_color_t c){
 if(c.hue < -180)c.hue=-180;
 if(c.hue>180)c.hue=180;
 if(c.saturation>160)c.saturation=160;
 if(c.detail>FRESLEST_TV_SHARP)c.detail=FRESLEST_TV_CLEAN;
 return c;
}
freslest_tv_color_t freslest_video_color_config(void){
 portENTER_CRITICAL(&lock);freslest_tv_color_t c=color;portEXIT_CRITICAL(&lock);return c;
}
void freslest_video_color_preview(freslest_tv_color_t c){
 c=color_clamp(c);portENTER_CRITICAL(&lock);
 if(c.hue!=color.hue||c.saturation!=color.saturation||c.detail!=color.detail){color=c;++color_revision;}
 portEXIT_CRITICAL(&lock);
}
bool freslest_video_color_save(freslest_tv_color_t c){
 c=color_clamp(c);nvs_handle_t n;esp_err_t e=nvs_open("fl_video",NVS_READWRITE,&n);
 if(e==ESP_OK){e=nvs_set_blob(n,"color_v2",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
 if(e==ESP_OK)freslest_video_color_preview(c);
 return e==ESP_OK;
}
static freslest_tv_config_t tv_clamp(freslest_tv_config_t c) {
 if(c.scale<80)c.scale=80;
 if(c.scale>100)c.scale=100;
 int mx=(VIDEO_W-VIDEO_W*c.scale/100)/2,my=(VIDEO_H-VIDEO_H*c.scale/100)/2;
 if(c.x < -mx)c.x=-mx;
 if(c.x>mx)c.x=mx;
 if(c.y < -my)c.y=-my;
 if(c.y>my)c.y=my;
 return c;
}
freslest_tv_config_t freslest_video_tv_config(void){
 portENTER_CRITICAL(&lock);freslest_tv_config_t c=tv;portEXIT_CRITICAL(&lock);return c;
}
void freslest_video_tv_preview(freslest_tv_config_t c){
 c=tv_clamp(c);portENTER_CRITICAL(&lock);tv=c;portEXIT_CRITICAL(&lock);
}
bool freslest_video_tv_save(freslest_tv_config_t c){
 c=tv_clamp(c);nvs_handle_t n;esp_err_t err=nvs_open("fl_video",NVS_READWRITE,&n);
 if(err==ESP_OK){err=nvs_set_blob(n,"tv_v1",&c,sizeof(c));if(err==ESP_OK)err=nvs_commit(n);nvs_close(n);}
 if(err==ESP_OK)freslest_video_tv_preview(c);
 return err==ESP_OK;
}
bool freslest_video_save_output(freslest_display_t value) {
 if(value!=FRESLEST_DISPLAY_LCD&&value!=FRESLEST_DISPLAY_COMPOSITE)return false;
 nvs_handle_t n;esp_err_t err=nvs_open("fl_video",NVS_READWRITE,&n);
 if(err==ESP_OK){err=nvs_set_u8(n,"output",value);if(err==ESP_OK)err=nvs_commit(n);nvs_close(n);}
 return err==ESP_OK;
}
freslest_display_t freslest_video_boot_output(void) {
 if(boot_read)return output;
 boot_read=true;uint8_t value=0;nvs_handle_t n;
 if(nvs_open("fl_video",NVS_READONLY,&n)==ESP_OK){
  nvs_get_u8(n,"output",&value);freslest_tv_config_t c;size_t bytes=sizeof(c);
  if(nvs_get_blob(n,"tv_v1",&c,&bytes)==ESP_OK&&bytes==sizeof(c)&&c.scale>=80&&c.scale<=100)
   tv=tv_clamp(c);
  freslest_tv_color_t cc;bytes=sizeof(cc);
  bool valid=nvs_get_blob(n,"color_v2",&cc,&bytes)==ESP_OK&&bytes==sizeof(cc)&&cc.hue>=-180&&cc.hue<=180&&cc.saturation<=160&&cc.detail<=FRESLEST_TV_SHARP;
  if(!valid){bytes=sizeof(cc);
   valid=nvs_get_blob(n,"color_v1",&cc,&bytes)==ESP_OK&&bytes==sizeof(cc)&&cc.hue>=-180&&cc.hue<=180&&cc.saturation<=160&&cc.detail==0;
  }
  if(valid)color=cc;
  nvs_close(n);
 }
 gpio_config_t cfg={.pin_bit_mask=1ULL<<16,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
 gpio_config(&cfg);vTaskDelay(pdMS_TO_TICKS(5));
 if(!gpio_get_level(16)){value=0;freslest_video_save_output(FRESLEST_DISPLAY_LCD);}
 output=value==1?FRESLEST_DISPLAY_COMPOSITE:FRESLEST_DISPLAY_LCD;
 return output;
}
freslest_display_t freslest_video_output(void){return output;}
static bool IRAM_ATTR on_field(void *ctx) {
 (void)ctx;vsyncs++;last_vsync_us=(uint32_t)esp_timer_get_time();BaseType_t wake=pdFALSE;
 xSemaphoreGiveFromISR(sync_sem,&wake);return wake==pdTRUE;
}
static void render_pending(void){
 // A queued field may still be waiting for the preceding ring's boundary.
 // Never edit it while DMA can see it; retry publication on the next EOF.
 if(prepared>=0){if(!freslest_av_publish(engine,prepared))return;prepared=-1;}
 portENTER_CRITICAL(&lock);freslest_tv_color_t c=color;uint32_t revision=color_revision;
 bool changed=revision!=rendered_color_revision;
 int source_slot=slots.ready>=0?slots.ready:changed?slots.reading:-1;
 portEXIT_CRITICAL(&lock);
 if(source_slot<0)return;
 int field=freslest_av_acquire(engine,&wave);if(field<0)return;
 portENTER_CRITICAL(&lock);int slot=slots.ready>=0?video_slots_consume(&slots):slots.reading;portEXIT_CRITICAL(&lock);
 if(slot<0){freslest_av_cancel(engine,field);return;}
 if(changed){av_signal_palette(phase,c.hue,c.saturation);rendered_color_revision=revision;}
 av_signal_burst(wave,c.saturation!=0);
 av_signal_encode(&renderer,wave,frames[slot],phase,c.detail);
 prepared=field;presentations++;
 if(freslest_av_publish(engine,prepared))prepared=-1;
}
static void pump(void *arg) {
 (void)arg;
 for(;;) {
  if(xSemaphoreTake(sync_sem,pdMS_TO_TICKS(50))!=pdTRUE)continue;
  render_pending();
  // A fixed yield keeps USB, buttons and the idle watchdog runnable even if
  // frame conversion takes longer than one field. No watchdog is disabled.
  vTaskDelay(1);
 }
}
bool freslest_video_begin(void) {
 if(freslest_video_boot_output()!=FRESLEST_DISPLAY_COMPOSITE)return true;
 if(started)return true;
 gpio_config_t bl={.pin_bit_mask=(1ULL<<41)|(1ULL<<10),.mode=GPIO_MODE_OUTPUT};
 gpio_config(&bl);gpio_set_level(41,0);gpio_set_level(10,1);
 video_slots_init(&slots);av_renderer_init(&renderer);av_signal_palette(phase,color.hue,color.saturation);rendered_color_revision=color_revision;prepared=-1;
 sync_sem=xSemaphoreCreateBinary();if(!sync_sem)goto fail;
 for(int i=0;i<3;i++){frames[i]=heap_caps_calloc(VIDEO_W*VIDEO_H,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!frames[i])goto fail;}
 freslest_av_config_t config={.on_field=on_field};
 if(freslest_av_open(&config,&engine)!=ESP_OK)goto fail;
 if(xTaskCreatePinnedToCore(pump,"freslest_av",6144,NULL,6,NULL,0)!=pdPASS)goto fail;
 started_us=esp_timer_get_time();first_frame_ready=false;started=true;
 ESP_LOGW("video","Freslest AV Engine " FRESLEST_AV_VERSION ": raw stream, whole-field handover; D0..7=8,13,14,2,42,40,39,48");return true;
fail:
 if(engine){freslest_av_close(engine);engine=NULL;}
 if(sync_sem){vSemaphoreDelete(sync_sem);sync_sem=NULL;}
 for(int i=0;i<3;i++){heap_caps_free(frames[i]);frames[i]=NULL;}
 wave=NULL;ESP_LOGE("video","Composite init failed. Hold BACK during reset for LCD.");return false;
}
bool freslest_video_healthy(void){return output==FRESLEST_DISPLAY_LCD||
 (started&&freslest_av_healthy(engine));}
bool freslest_video_starting(void){
 if(output!=FRESLEST_DISPLAY_COMPOSITE||!started||first_frame_ready)return false;
 if(freslest_video_healthy()){first_frame_ready=true;return false;}
 return esp_timer_get_time()-started_us<10000000;
}
static void submit(const void *src,int w,int h,const uint32_t *pal,bool swapped,bool cursor,int x,int y) {
 if(!started||!src||w<1||h<1||w>480||h>360)return;
 portENTER_CRITICAL(&lock);int slot=video_slots_acquire(&slots);portEXIT_CRITICAL(&lock);
 if(slot<0)return;
 freslest_tv_config_t c=freslest_video_tv_config();
 video_convert(frames[slot],src,w,h,pal,swapped,cursor,x,y,&c);
 // The other slots are immutable to the single producer during this call.
 // Compare with the newest complete image; leave the DMA wave untouched when
 // the UI/pointer is stationary instead of re-encoding it sixty times/second.
 portENTER_CRITICAL(&lock);int latest=slots.ready>=0?slots.ready:slots.reading;portEXIT_CRITICAL(&lock);
 if(latest>=0&&!memcmp(frames[slot],frames[latest],VIDEO_W*VIDEO_H)){
  portENTER_CRITICAL(&lock);slots.writing=-1;portEXIT_CRITICAL(&lock);unchanged_frames++;return;
 }
 portENTER_CRITICAL(&lock);video_slots_publish(&slots,slot);portEXIT_CRITICAL(&lock);
}
void freslest_video_rgb565(const uint16_t *s,int w,int h,bool swap,bool cursor,int x,int y){submit(s,w,h,NULL,swap,cursor,x,y);}
void freslest_video_indexed(const uint8_t *s,int w,int h,const uint32_t *pal,bool cursor,int x,int y){if(pal)submit(s,w,h,pal,false,cursor,x,y);}

void freslest_video_diagnostics(void){
 freslest_tv_config_t c=freslest_video_tv_config();
 freslest_av_stats_t s;freslest_av_stats(engine,&s);
 ESP_LOGW("video","AV engine=" FRESLEST_AV_VERSION " fields=%lu swaps=%lu errors=%lu unknown=%lu running=%u active=%d pending=%d field-dt=%lu us max=%lu us; predicted sample=%lu Hz",
  (unsigned long)s.fields,(unsigned long)s.swaps,(unsigned long)s.descriptor_errors,(unsigned long)s.unknown_eof,s.running,s.active,s.pending,
  (unsigned long)s.last_period_us,(unsigned long)s.max_period_us,(unsigned long)s.sample_hz);
 ESP_LOGW("video","output=%s started=%u vsync=%lu images=%lu same=%lu last-sync-age=%lu us; D0..7=8,13,14,2,42,40,39,48",
  output==FRESLEST_DISPLAY_LCD?"LCD":"COMPOSITE",started,(unsigned long)vsyncs,(unsigned long)presentations,
  (unsigned long)unchanged_frames,(unsigned long)((uint32_t)esp_timer_get_time()-last_vsync_us));
 ESP_LOGW("video","TV size=%u%% horizontal=%d vertical=%d first-frame=%u starting=%u",
  c.scale,c.x,c.y,first_frame_ready,freslest_video_starting());
 freslest_tv_color_t cc=freslest_video_color_config();
 ESP_LOGW("video","AV hue=%d deg color=%u%% detail=%s burst=%s; NTSC 227.5 cycles/line; alternating burst/chroma; neutral greys",cc.hue,cc.saturation,cc.detail==FRESLEST_TV_SHARP?"SHARP":"CLEAN",cc.saturation?"ON":"OFF");
}
