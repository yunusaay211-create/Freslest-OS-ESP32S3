// Doom-only VGA pacing and successful-frame cache. Logical size stays 320x240.
#include "doom_vga_present.h"
#include "freslest_video.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
static uint8_t *previous;
static bool allocated,valid,last_cursor;
static int last_x,last_y;
static unsigned last_fit;
static uint32_t generation,last_generation;
static int64_t next_frame_us;
void doom_vga_palette_changed(void){++generation;}
void doom_vga_send_frame(const uint8_t *pixels,bool cursor,int x,int y){
 if(!pixels)return;
 int64_t now=esp_timer_get_time();
 if(now<next_frame_us)vTaskDelay(pdMS_TO_TICKS((next_frame_us-now+999)/1000));
 next_frame_us=esp_timer_get_time()+33333; // at most 30 rendered submissions per second
 if(!allocated){allocated=true;previous=heap_caps_malloc(320*240,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
 unsigned fit=freslest_video_fit();
 if(previous&&valid&&generation==last_generation&&fit==last_fit&&cursor==last_cursor&&
    (!cursor||(x==last_x&&y==last_y))&&!memcmp(previous,pixels,320*240))return;
 if(!doom_vga_try_present(pixels,cursor,x,y))return; // dropped frames remain dirty
 if(previous){memcpy(previous,pixels,320*240);valid=true;}
 last_generation=generation;last_fit=fit;last_cursor=cursor;last_x=x;last_y=y;
}
