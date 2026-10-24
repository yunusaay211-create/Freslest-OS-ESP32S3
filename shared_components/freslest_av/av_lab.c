// SPDX-License-Identifier: MIT
#include "freslest_av.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
bool freslest_av_lab_requested(void){
 gpio_config_t c={.pin_bit_mask=(1ULL<<4)|(1ULL<<16),.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
 if(gpio_config(&c)!=ESP_OK)return false;
 vTaskDelay(pdMS_TO_TICKS(5));
 return !gpio_get_level(4)&&gpio_get_level(16); // BACK retains LCD recovery priority.
}
static void stats(freslest_av_handle_t e){
 freslest_av_stats_t s;freslest_av_stats(e,&s);
 printf("AVLAB: fields=%lu swaps=%lu descriptor-errors=%lu unknown-EOF=%lu running=%u active=%d pending=%d; EOF period=%lu us max=%lu us; divider prediction=%lu Hz\n",
  (unsigned long)s.fields,(unsigned long)s.swaps,(unsigned long)s.descriptor_errors,(unsigned long)s.unknown_eof,s.running,s.active,s.pending,
  (unsigned long)s.last_period_us,(unsigned long)s.max_period_us,(unsigned long)s.sample_hz);
}
void freslest_av_lab_run(int (*read_key)(void)){
 const int keys[]={4,5,6,7,15,16,3};uint64_t mask=0;
 for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);i++)mask|=1ULL<<keys[i];
 gpio_config_t c={.pin_bit_mask=mask,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};gpio_config(&c);
 gpio_config_t lcd={.pin_bit_mask=(1ULL<<41)|(1ULL<<10),.mode=GPIO_MODE_OUTPUT};gpio_config(&lcd);gpio_set_level(41,0);gpio_set_level(10,1);
 printf("\nFRESLEST AV ENGINE %s / STATIC LAB\nD0..D7 = 8,13,14,2,42,40,39,48\n",FRESLEST_AV_VERSION);
 puts("UP/DOWN: pattern. OK: repeat. BACK: return to OS after release. UART: 0=mono 1=bars 2=text 3=white i=stats b=back.");
 puts("First test is MONO: no colour burst. No OS UI, games, USB, Bluetooth or audio service runs in this lab.");
 puts("This lab does not confirm a pending OTA trial. Return to normal OS to complete startup confirmation.");
 freslest_av_handle_t engine=NULL;esp_err_t err=freslest_av_open(NULL,&engine);
 uint8_t *canvas=heap_caps_malloc(AV_WIDTH*AV_HEIGHT,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 av_renderer_t *renderer=heap_caps_malloc(sizeof(*renderer),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 if(err==ESP_OK&&(!canvas||!renderer))err=ESP_ERR_NO_MEM;
 if(err!=ESP_OK){printf("AVLAB initialization failed: %s. BACK returns to OS.\n",esp_err_to_name(err));}
 else av_renderer_init(renderer);
 unsigned pattern=0;int prepared=-1;bool dirty=true,released=false,exit=false;
 uint32_t previous=0,stable=0,last_change=0;uint8_t palette[256][4];
 while(!exit){
  uint32_t now=(uint32_t)(esp_timer_get_time()/1000),pressed=0;
  for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);i++)if(!gpio_get_level(keys[i]))pressed|=1u<<i;
  if(pressed!=previous){previous=pressed;last_change=now;}
  uint32_t edges=0;
  if(now-last_change>=25&&pressed!=stable){edges=pressed&~stable;stable=pressed;}
  if(!pressed)released=true;
  int key=read_key?read_key():-1;
  if(released){if(edges&1){pattern=(pattern+3)%4;dirty=true;}if(edges&2){pattern=(pattern+1)%4;dirty=true;}}
  if(released&&(edges&(1u<<4)))dirty=true;
  if((released&&(edges&(1u<<5)))||key=='b')exit=true;
  if(key>='0'&&key<='3'){pattern=key-'0';dirty=true;}
  if(key=='i')stats(engine);
  if(err==ESP_OK&&canvas&&renderer){
   if(prepared>=0&&freslest_av_publish(engine,prepared))prepared=-1;
   if(dirty&&prepared<0){
    uint8_t *wave;int slot=freslest_av_acquire(engine,&wave);
    if(slot>=0){
     bool colour=pattern==1;av_signal_pattern(canvas,pattern);av_signal_palette(palette,0,colour?100:0);
     av_signal_burst(wave,colour);av_signal_encode(renderer,wave,canvas,palette,1);
     prepared=slot;dirty=false;
     printf("AVLAB pattern=%u (%s), burst=%s; field remains immutable while idle\n",pattern,
      pattern==0?"MONO GRID":pattern==1?"COLOUR BARS":pattern==2?"MONO TEXT":"WHITE",colour?"ON":"OFF");
    }
   }
  }
  vTaskDelay(pdMS_TO_TICKS(10));
 }
 if(prepared>=0)freslest_av_cancel(engine,prepared);
 freslest_av_close(engine);heap_caps_free(canvas);heap_caps_free(renderer);
 // Release UP and BACK before reset, so recovery/diagnostic boot choices are deliberate.
 while(!gpio_get_level(4)||!gpio_get_level(16))vTaskDelay(pdMS_TO_TICKS(10));
 esp_restart();
}
