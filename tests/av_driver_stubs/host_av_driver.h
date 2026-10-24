#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "av_signal.h"
typedef int esp_err_t,soc_module_clk_t,portMUX_TYPE;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_NO_MEM 3
#define ESP_ERR_NOT_FOUND 4
#define ESP_ERR_INVALID_STATE 5
#define ESP_ERR_INVALID_SIZE 6
#define IRAM_ATTR
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
#define ESP_CACHE_MSYNC_FLAG_DIR_C2M 1
#define ESP_CACHE_MSYNC_FLAG_UNALIGNED 2
#define LCD_CLK_SRC_PLL240M 240
#define ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED 1
#define LCD_COM_DEVICE_TYPE_I80 1
#define LCD_LL_EVENT_I80 2
#define GPIO_MODE_OUTPUT 1
#define GPIO_DRIVE_CAP_1 1
#define PIN_FUNC_GPIO 1
#define LCD_DATA_OUT0_IDX 133
#define GDMA_CHANNEL_DIRECTION_TX 1
#define GDMA_TRIG_PERIPH_LCD 1
#define GDMA_MAKE_TRIGGER(a,b) ((a)+(b))
#define ESP_PM_NO_LIGHT_SLEEP 1
#define CONFIG_PM_ENABLE 1
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define portENTER_CRITICAL_ISR(p) ((void)(p))
#define portEXIT_CRITICAL_ISR(p) ((void)(p))
#define LCD_CLOCK_SRC_ATOMIC() if(true)
#define PERIPH_RCC_ATOMIC() if(true)
#define PERIPH_RCC_ACQUIRE_ATOMIC(mod,ref) for(int ref=mock_power_refs++,once=1;once;once=0)
#define PERIPH_RCC_RELEASE_ATOMIC(mod,ref) for(int ref=--mock_power_refs,once=1;once;once=0)
static int mock_ops,mock_fail_at,mock_memory,mock_channels,mock_lists,mock_claims,mock_power_refs,mock_sources,mock_locks;
static int64_t mock_time;
static bool mock_step(void){mock_ops++;return mock_ops!=mock_fail_at;}
static void *heap_caps_calloc(size_t n,size_t z,int caps){assert(caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));if(!mock_step())return NULL;void *p=calloc(n,z);assert(p);mock_memory++;return p;}
static void *heap_caps_aligned_alloc(size_t align,size_t size,int caps){assert(align==64&&size==238464&&caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!mock_step())return NULL;void *p=aligned_alloc(align,size);assert(p);mock_memory++;return p;}
static void heap_caps_free(void *p){if(p){free(p);mock_memory--;}}
static int esp_cache_msync(void *p,size_t n,int flags){assert(p&&((uintptr_t)p%64)==0&&n<=238464&&(flags&1));return mock_step()?0:-1;}
static int lcd_com_register_device(int type,void *p){assert(type==1&&p);if(!mock_step())return -1;mock_claims++;return 0;}
static void lcd_com_remove_device(int type,int id){assert(type==1&&id==0);mock_claims--;}
static struct {struct {int module;}buses[1];}lcd_periph_i80_signals;
typedef struct {struct {unsigned lcd_start;}lcd_user;bool rgb,convert,swizzle,reverse,always,next;int stride,cmd,dummy,data,divider,denom,numer;} mock_lcd_t;
static mock_lcd_t mock_lcd;
typedef struct {mock_lcd_t *dev;}lcd_hal_context_t;
typedef struct {unsigned integer,denominator,numerator;}hal_utils_clk_div_t;
static void lcd_hal_init(lcd_hal_context_t *h,int id){assert(!id);h->dev=&mock_lcd;}
static void lcd_ll_enable_bus_clock(int id,bool b){assert(!id);(void)b;}
static void lcd_ll_reset_register(int id){assert(!id);memset(&mock_lcd,0,sizeof(mock_lcd));}
static void lcd_ll_enable_clock(mock_lcd_t *d,bool b){assert(d==&mock_lcd);(void)b;}
static void lcd_ll_select_clk_src(mock_lcd_t *d,int src){assert(d&&src==240);}
static int esp_clk_tree_src_get_freq_hz(int src,int precision,uint32_t *hz){assert(src==240&&precision==1);*hz=240000000;return mock_step()?0:-1;}
static int esp_clk_tree_enable_src(int src,bool on){assert(src==240);if(on){if(!mock_step())return -1;mock_sources++;}else mock_sources--;return 0;}
static unsigned lcd_hal_cal_pclk_freq(lcd_hal_context_t *h,unsigned src,unsigned expect,hal_utils_clk_div_t *d){assert(h&&src==240000000&&expect==14318181);d->integer=8;d->denominator=63;d->numerator=24;return 14318182;}
static void lcd_ll_set_group_clock_coeff(mock_lcd_t *d,int n,int a,int b){d->divider=n;d->denom=a;d->numer=b;}
static void lcd_ll_reset(mock_lcd_t *d){d->lcd_user.lcd_start=0;}
static void lcd_ll_fifo_reset(mock_lcd_t *d){assert(d);}
static void lcd_ll_enable_rgb_mode(mock_lcd_t *d,bool b){d->rgb=b;}
static void lcd_ll_enable_rgb_yuv_convert(mock_lcd_t *d,bool b){d->convert=b;}
static void lcd_ll_set_dma_read_stride(mock_lcd_t *d,unsigned n){d->stride=n;}
static void lcd_ll_reverse_dma_data_bit_order(mock_lcd_t *d,bool b){d->reverse=b;}
static void lcd_ll_enable_swizzle(mock_lcd_t *d,bool b){d->swizzle=b;}
static void lcd_ll_set_clock_idle_level(mock_lcd_t *d,bool b){assert(d&&!b);}
static void lcd_ll_set_pixel_clock_edge(mock_lcd_t *d,bool b){assert(d&&!b);}
static void lcd_ll_enable_auto_next_frame(mock_lcd_t *d,bool b){d->next=b;}
static void lcd_ll_set_phase_cycles(mock_lcd_t *d,unsigned c,unsigned x,unsigned n){d->cmd=c;d->dummy=x;d->data=n;}
static void lcd_ll_set_blank_cycles(mock_lcd_t *d,unsigned a,unsigned b){assert(d&&a==1&&b==1);}
static void lcd_ll_enable_output_always_on(mock_lcd_t *d,bool b){d->always=b;}
static void lcd_ll_enable_interrupt(mock_lcd_t *d,unsigned n,bool b){assert(d&&n==2&&!b);}
static void lcd_ll_clear_interrupt_status(mock_lcd_t *d,unsigned n){assert(d&&n==UINT32_MAX);}
static void lcd_ll_start(mock_lcd_t *d){d->lcd_user.lcd_start=1;}
static void lcd_ll_stop(mock_lcd_t *d){d->lcd_user.lcd_start=0;}
typedef struct {uint64_t pin_bit_mask;int mode;}gpio_config_t;
static uint64_t mock_pin_mask;static int mock_routes;
static int gpio_config(const gpio_config_t *c){uint64_t expect=0;const int pins[]={8,13,14,2,42,40,39,48};for(int i=0;i<8;i++)expect|=1ULL<<pins[i];assert(c->pin_bit_mask==expect&&c->mode==1);mock_pin_mask=c->pin_bit_mask;return mock_step()?0:-1;}
static int gpio_set_level(int pin,int level){assert((mock_pin_mask&(1ULL<<pin))&&!level);return mock_step()?0:-1;}
static int gpio_set_drive_capability(int pin,int cap){assert((mock_pin_mask&(1ULL<<pin))&&cap==1);return mock_step()?0:-1;}
static void gpio_func_sel(int pin,int fn){assert((mock_pin_mask&(1ULL<<pin))&&fn==1);}
static void esp_rom_gpio_connect_out_signal(int pin,int signal,bool inv,bool en){const int pins[]={8,13,14,2,42,40,39,48};assert(signal>=133&&signal<141&&pin==pins[signal-133]&&!inv&&!en);mock_routes++;}
typedef struct dma_descriptor_s {struct {unsigned size,length,suc_eof,owner;}dw0;void *buffer;struct dma_descriptor_s *next;}dma_descriptor_t;
typedef struct gdma_link_list_t {dma_descriptor_t *nodes;unsigned count;}*gdma_link_list_handle_t;
typedef struct {intptr_t tx_eof_desc_addr;}gdma_event_data_t;
typedef struct mock_dma *gdma_channel_handle_t;
typedef bool(*gdma_event_callback_t)(gdma_channel_handle_t,gdma_event_data_t*,void*);
typedef struct {gdma_event_callback_t on_trans_eof,on_descr_err;}gdma_tx_event_callbacks_t;
struct mock_dma{bool connected,running;gdma_tx_event_callbacks_t callbacks;void *context;uintptr_t head;};
typedef struct {int direction;struct{unsigned isr_cache_safe;}flags;}gdma_channel_alloc_config_t;
typedef struct {bool owner_check,auto_update_desc,eof_till_data_popped;}gdma_strategy_config_t;
typedef struct {unsigned max_data_burst_size;bool access_ext_mem;}gdma_transfer_config_t;
typedef struct {unsigned num_items;size_t item_alignment;struct {bool check_owner;}flags;}gdma_link_list_config_t;
typedef struct {void *buffer;size_t buffer_alignment,length;struct{bool mark_eof,mark_final;}flags;}gdma_buffer_mount_config_t;
static int gdma_new_ahb_channel(const gdma_channel_alloc_config_t *c,gdma_channel_handle_t *e){assert(c->direction==1&&c->flags.isr_cache_safe);if(!mock_step())return -1;*e=calloc(1,sizeof(**e));mock_channels++;return 0;}
static int gdma_connect(gdma_channel_handle_t e,int periph){assert(e&&periph==1);if(!mock_step())return -1;e->connected=true;return 0;}
static int gdma_apply_strategy(gdma_channel_handle_t e,const gdma_strategy_config_t *s){assert(e&&!s->owner_check&&!s->auto_update_desc&&s->eof_till_data_popped);return mock_step()?0:-1;}
static int gdma_config_transfer(gdma_channel_handle_t e,const gdma_transfer_config_t *c){assert(e&&c->max_data_burst_size==64&&c->access_ext_mem);return mock_step()?0:-1;}
static int gdma_get_alignment_constraints(gdma_channel_handle_t e,size_t *a,size_t *b){assert(e);*a=4;*b=64;return mock_step()?0:-1;}
static int gdma_new_link_list(const gdma_link_list_config_t *c,gdma_link_list_handle_t *l){assert(c->num_items==60&&c->item_alignment==4&&!c->flags.check_owner);if(!mock_step())return -1;*l=calloc(1,sizeof(**l));(*l)->count=60;(*l)->nodes=calloc(60,sizeof(dma_descriptor_t));mock_lists++;return 0;}
static int gdma_link_mount_buffers(gdma_link_list_handle_t l,int at,const gdma_buffer_mount_config_t *c,unsigned num,int *end){assert(l&&!at&&num==1&&!end&&c->length==238420&&c->buffer_alignment==64&&c->flags.mark_eof&&!c->flags.mark_final);if(!mock_step())return -1;
 size_t left=c->length,offset=0;for(unsigned i=0;i<l->count;i++){unsigned n=left>4032?4032:left;l->nodes[i].dw0.size=l->nodes[i].dw0.length=n;l->nodes[i].dw0.owner=1;l->nodes[i].dw0.suc_eof=i+1==l->count;l->nodes[i].buffer=(uint8_t*)c->buffer+offset;l->nodes[i].next=&l->nodes[(i+1)%l->count];offset+=n;left-=n;}assert(!left);return 0;}
static uintptr_t gdma_link_get_head_addr(gdma_link_list_handle_t l){return (uintptr_t)l->nodes;}
static int gdma_register_tx_event_callbacks(gdma_channel_handle_t e,const gdma_tx_event_callbacks_t *c,void *ctx){assert(e&&c->on_trans_eof&&c->on_descr_err&&ctx);if(!mock_step())return -1;e->callbacks=*c;e->context=ctx;return 0;}
static int gdma_start(gdma_channel_handle_t e,uintptr_t head){assert(e->connected&&head);if(!mock_step())return -1;e->running=true;e->head=head;return 0;}
static void gdma_stop(gdma_channel_handle_t e){e->running=false;}
static void gdma_disconnect(gdma_channel_handle_t e){assert(e->connected&&!e->running);e->connected=false;}
static void gdma_del_channel(gdma_channel_handle_t e){assert(!e->connected&&!e->running);free(e);mock_channels--;}
static void gdma_del_link_list(gdma_link_list_handle_t l){free(l->nodes);free(l);mock_lists--;}
static int gdma_link_concat(gdma_link_list_handle_t a,int tail,gdma_link_list_handle_t b,int head){assert(tail==-1&&head==0);if(!mock_step())return -1;a->nodes[a->count-1].next=b->nodes;return 0;}
typedef void *esp_pm_lock_handle_t;
static int esp_pm_lock_create(int type,int n,const char *name,esp_pm_lock_handle_t *p){assert(type==1&&!n&&!strcmp(name,"freslest_av"));if(!mock_step())return -1;*p=malloc(1);mock_locks++;return 0;}
static int esp_pm_lock_acquire(esp_pm_lock_handle_t p){assert(p);return mock_step()?0:-1;}
static void esp_pm_lock_release(esp_pm_lock_handle_t p){assert(p);}
static void esp_pm_lock_delete(esp_pm_lock_handle_t p){free(p);mock_locks--;}
static int64_t esp_timer_get_time(void){return mock_time;}
static void esp_rom_delay_us(unsigned n){assert(n==4);mock_time+=n;}
static const char *esp_err_to_name(int e){return e?"ERROR":"OK";}
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
