#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 2
#define IRAM_ATTR
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define NVS_READWRITE 1
#define NVS_READONLY 0
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_ENABLE 1
#define GPIO_DRIVE_CAP_1 1
#define pdFALSE 0
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static inline void host_log(const char *tag,const char *format,...){assert(tag&&format);}
#define ESP_LOGE(...) host_log(__VA_ARGS__)
#define ESP_LOGW(...) host_log(__VA_ARGS__)
#define LCD_CLK_SRC_PLL240M 240
#define ESP_CACHE_MSYNC_FLAG_DIR_C2M 1
#define ESP_CACHE_MSYNC_FLAG_UNALIGNED 2
typedef int BaseType_t,portMUX_TYPE,nvs_handle_t;
typedef void *SemaphoreHandle_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en;} gpio_config_t;
static int stored,back=1,nvs_fail,stage,alloc_fail,allocations,live,live_panels,live_sem,task_calls;
static int64_t host_time;
static uint64_t configured_pins;
static uint8_t stored_tv[3],stored_color[4],stored_color_v2[4];static size_t stored_tv_size,stored_color_size,stored_color_v2_size;
static int nvs_open(const char *name,int mode,nvs_handle_t *n){assert(!strcmp(name,"fl_video"));(void)mode;*n=1;return nvs_fail==1?-1:0;}
static int nvs_get_u8(nvs_handle_t n,const char *key,uint8_t *v){(void)n;assert(!strcmp(key,"output"));*v=stored;return 0;}
static int nvs_set_u8(nvs_handle_t n,const char *key,uint8_t v){(void)n;assert(!strcmp(key,"output"));if(nvs_fail==2)return -1;stored=v;return 0;}
static int nvs_get_blob(nvs_handle_t n,const char *key,void *v,size_t *size){
 (void)n;bool old=!strcmp(key,"color_v1"),color=!strcmp(key,"color_v2");assert(old||color||!strcmp(key,"tv_v1"));
 size_t bytes=color?stored_color_v2_size:old?stored_color_size:stored_tv_size;if(!bytes)return -1;
 if(*size<bytes)return -1;memcpy(v,color?stored_color_v2:old?stored_color:stored_tv,bytes);*size=bytes;return 0;
}
static int nvs_set_blob(nvs_handle_t n,const char *key,const void *v,size_t size){
 (void)n;bool c=!strcmp(key,"color_v2");assert(c||!strcmp(key,"tv_v1"));assert(size==(c?4:3));if(nvs_fail==2)return -1;
 if(c){memcpy(stored_color_v2,v,size);stored_color_v2_size=size;}else{memcpy(stored_tv,v,size);stored_tv_size=size;}return 0;
}
static int nvs_commit(nvs_handle_t n){(void)n;return nvs_fail==3?-1:0;}
static void nvs_close(nvs_handle_t n){(void)n;}
static int gpio_config(const gpio_config_t *c){configured_pins|=c->pin_bit_mask;return 0;}
static int gpio_get_level(int pin){assert(pin==16);return back;}
static int gpio_set_level(int pin,int v){assert((pin==41&&!v)||(pin==10&&v));return 0;}
static void vTaskDelay(int ms){host_time+=ms*1000;}
static int64_t esp_timer_get_time(void){return host_time;}
static void *heap_caps_calloc(size_t n,size_t s,int caps){assert(caps==3);allocations++;if(allocations==alloc_fail)return NULL;live++;return calloc(n,s);}
static void heap_caps_free(void *p){if(p){live--;free(p);}}
static SemaphoreHandle_t xSemaphoreCreateBinary(void){if(stage==7)return NULL;live_sem++;return (void*)1;}
static void vSemaphoreDelete(SemaphoreHandle_t s){assert(s);live_sem--;}
static int xSemaphoreTake(SemaphoreHandle_t s,int t){assert(s);(void)t;return 0;}
static void xSemaphoreGiveFromISR(SemaphoreHandle_t s,BaseType_t *w){assert(s);*w=0;}
static int xTaskCreatePinnedToCore(void (*f)(void*),const char *name,int size,void *arg,int priority,void *out,int core){assert(f&&size==6144&&priority==6&&core==0);(void)name;(void)arg;(void)out;task_calls++;return stage==6?0:1;}
#include "freslest_av.h"
#include "av_ownership.h"
struct freslest_av {uint8_t *waves[3];av_ownership_t ownership;freslest_av_stats_t stats;freslest_av_config_t config;};
esp_err_t freslest_av_open(const freslest_av_config_t *c,freslest_av_handle_t *out){
 if(stage>=1&&stage<=5)return -1;
 *out=calloc(1,sizeof(**out));(*out)->config=*c;av_ownership_init(&(*out)->ownership);
 for(int i=0;i<3;i++){(*out)->waves[i]=malloc(AV_FIELD_BYTES);assert((*out)->waves[i]);av_signal_prime((*out)->waves[i],false);}
 (*out)->stats.running=true;live_panels++;return 0;
}
void freslest_av_close(freslest_av_handle_t e){for(int i=0;i<3;i++)free(e->waves[i]);free(e);live_panels--;}
int freslest_av_acquire(freslest_av_handle_t e,uint8_t **wave){int i=av_ownership_acquire(&e->ownership);*wave=i>=0?e->waves[i]:NULL;return i;}
bool freslest_av_publish(freslest_av_handle_t e,int i){return av_ownership_queue(&e->ownership,i);}
void freslest_av_cancel(freslest_av_handle_t e,int i){av_ownership_cancel(&e->ownership,i);}
void freslest_av_stats(freslest_av_handle_t e,freslest_av_stats_t *s){memset(s,0,sizeof(*s));if(e){*s=e->stats;s->swaps=e->ownership.swaps;}}
bool freslest_av_healthy(freslest_av_handle_t e){return e&&e->stats.fields>2&&e->ownership.swaps>0&&!e->stats.descriptor_errors&&(uint32_t)((uint32_t)host_time-e->stats.last_field_us)<500000;}
static void host_av_field(freslest_av_handle_t e){
 e->stats.fields++;e->stats.last_field_us=host_time;
 av_ownership_complete(&e->ownership,e->ownership.pending);
 e->config.on_field(e->config.context);
}
