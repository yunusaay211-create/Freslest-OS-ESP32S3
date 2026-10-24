// SPDX-License-Identifier: MIT
// Freslest AV 0.1: LCD_CAM raw i80 stream, explicit whole-field GDMA rings.
// ESP-IDF v5.4.4 / ESP32-S3. No RGB panel driver or hardware RGB porches.
#include "freslest_av.h"
#include "av_ownership.h"
#include <string.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_clk_tree.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "esp_private/esp_clk_tree_common.h"
#include "esp_private/gdma.h"
#include "esp_private/gdma_link.h"
#include "esp_private/periph_ctrl.h"
#include "esp_private/gpio.h"
#include "esp_lcd_common.h"
#include "hal/lcd_hal.h"
#include "hal/lcd_ll.h"
#include "hal/dma_types.h"
#include "soc/lcd_periph.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// D0..D7; VIDEO OUT -> far end is D7..D0: 48,39,40,42,2,14,13,8.
const int freslest_av_gpio[8]={8,13,14,2,42,40,39,48};
struct freslest_av {
 lcd_hal_context_t hal;
 gdma_channel_handle_t dma;
 gdma_link_list_handle_t links[3];
 uint8_t *wave[3];
 intptr_t tail[3];
 portMUX_TYPE mux;
 av_ownership_t ownership;
 freslest_av_stats_t stats;
 freslest_av_config_t config;
 esp_pm_lock_handle_t pm;
 int bus_id;
 bool powered,connected,source_enabled,pm_held;
};
static bool IRAM_ATTR field_eof(gdma_channel_handle_t channel,gdma_event_data_t *event,void *context){
 (void)channel;struct freslest_av *e=context;int slot=-1;
 for(int i=0;i<3;i++)if(e->tail[i]==event->tx_eof_desc_addr)slot=i;
 uint32_t now=(uint32_t)esp_timer_get_time();
 portENTER_CRITICAL_ISR(&e->mux);
 if(slot<0)e->stats.unknown_eof++;
 else{
  av_ownership_complete(&e->ownership,slot);
  if(e->stats.fields){uint32_t dt=now-e->stats.last_field_us;e->stats.last_period_us=dt;if(dt>e->stats.max_period_us)e->stats.max_period_us=dt;}
  e->stats.last_field_us=now;e->stats.fields++;
 }
 portEXIT_CRITICAL_ISR(&e->mux);
 return e->config.on_field?e->config.on_field(e->config.context):false;
}
static bool IRAM_ATTR descriptor_error(gdma_channel_handle_t channel,gdma_event_data_t *event,void *context){
 (void)channel;(void)event;struct freslest_av *e=context;
 portENTER_CRITICAL_ISR(&e->mux);e->stats.descriptor_errors++;portEXIT_CRITICAL_ISR(&e->mux);
 return e->config.on_field?e->config.on_field(e->config.context):false;
}
void freslest_av_close(freslest_av_handle_t e){
 if(!e)return;
 if(e->powered)lcd_ll_stop(e->hal.dev);
 if(e->dma){gdma_stop(e->dma);if(e->connected)gdma_disconnect(e->dma);gdma_del_channel(e->dma);}
 for(int i=0;i<3;i++){if(e->links[i])gdma_del_link_list(e->links[i]);heap_caps_free(e->wave[i]);}
 if(e->pm){if(e->pm_held)esp_pm_lock_release(e->pm);esp_pm_lock_delete(e->pm);}
 if(e->source_enabled)esp_clk_tree_enable_src((soc_module_clk_t)LCD_CLK_SRC_PLL240M,false);
 if(e->powered){
  LCD_CLOCK_SRC_ATOMIC(){lcd_ll_enable_clock(e->hal.dev,false);}
  PERIPH_RCC_RELEASE_ATOMIC(lcd_periph_i80_signals.buses[e->bus_id].module,ref_count){
   if(ref_count==0){lcd_ll_enable_bus_clock(e->bus_id,false);}
  }
 }
 if(e->bus_id>=0)lcd_com_remove_device(LCD_COM_DEVICE_TYPE_I80,e->bus_id);
 heap_caps_free(e);
}
esp_err_t freslest_av_open(const freslest_av_config_t *config,freslest_av_handle_t *out){
 if(!out)return ESP_ERR_INVALID_ARG;
 *out=NULL;
 struct freslest_av *e=heap_caps_calloc(1,sizeof(*e),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 if(!e)return ESP_ERR_NO_MEM;
 e->bus_id=-1;e->mux=(portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
 av_ownership_init(&e->ownership);if(config)e->config=*config;
 esp_err_t result=ESP_FAIL;
#define AV_CHECK(call) do{result=(call);if(result!=ESP_OK)goto fail;}while(0)
 e->bus_id=lcd_com_register_device(LCD_COM_DEVICE_TYPE_I80,e);
 if(e->bus_id<0){result=ESP_ERR_NOT_FOUND;goto fail;}
 PERIPH_RCC_ACQUIRE_ATOMIC(lcd_periph_i80_signals.buses[e->bus_id].module,ref_count){
  if(ref_count==0){lcd_ll_enable_bus_clock(e->bus_id,true);lcd_ll_reset_register(e->bus_id);}
 }
 e->powered=true;lcd_hal_init(&e->hal,e->bus_id);
 uint32_t source_hz=0;
 AV_CHECK(esp_clk_tree_src_get_freq_hz((soc_module_clk_t)LCD_CLK_SRC_PLL240M,ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,&source_hz));
 AV_CHECK(esp_clk_tree_enable_src((soc_module_clk_t)LCD_CLK_SRC_PLL240M,true));e->source_enabled=true;
 LCD_CLOCK_SRC_ATOMIC(){lcd_ll_enable_clock(e->hal.dev,true);lcd_ll_select_clk_src(e->hal.dev,LCD_CLK_SRC_PLL240M);}
 hal_utils_clk_div_t divider;
 e->stats.sample_hz=lcd_hal_cal_pclk_freq(&e->hal,source_hz,AV_SAMPLE_HZ,&divider);
 if(!e->stats.sample_hz){result=ESP_ERR_INVALID_STATE;goto fail;}
 LCD_CLOCK_SRC_ATOMIC(){lcd_ll_set_group_clock_coeff(e->hal.dev,divider.integer,divider.denominator,divider.numerator);}
 lcd_ll_reset(e->hal.dev);lcd_ll_fifo_reset(e->hal.dev);
 lcd_ll_enable_rgb_mode(e->hal.dev,false);lcd_ll_enable_rgb_yuv_convert(e->hal.dev,false);
 lcd_ll_set_dma_read_stride(e->hal.dev,8);lcd_ll_reverse_dma_data_bit_order(e->hal.dev,false);lcd_ll_enable_swizzle(e->hal.dev,false);
 lcd_ll_set_clock_idle_level(e->hal.dev,false);lcd_ll_set_pixel_clock_edge(e->hal.dev,false);
 lcd_ll_enable_auto_next_frame(e->hal.dev,false);
 // One startup dummy cycle primes the TX FIFO; no phase restart at field boundaries.
 lcd_ll_set_phase_cycles(e->hal.dev,0,1,1);lcd_ll_set_blank_cycles(e->hal.dev,1,1);
 lcd_ll_enable_output_always_on(e->hal.dev,true);
 PERIPH_RCC_ATOMIC(){lcd_ll_enable_interrupt(e->hal.dev,LCD_LL_EVENT_I80,false);}
 lcd_ll_clear_interrupt_status(e->hal.dev,UINT32_MAX);
 uint64_t pin_mask=0;for(int i=0;i<8;i++)pin_mask|=1ULL<<freslest_av_gpio[i];
 gpio_config_t gpio={.pin_bit_mask=pin_mask,.mode=GPIO_MODE_OUTPUT};AV_CHECK(gpio_config(&gpio));
 for(int i=0;i<8;i++){
  AV_CHECK(gpio_set_level(freslest_av_gpio[i],0));AV_CHECK(gpio_set_drive_capability(freslest_av_gpio[i],GPIO_DRIVE_CAP_1));
  gpio_func_sel(freslest_av_gpio[i],PIN_FUNC_GPIO);
  esp_rom_gpio_connect_out_signal(freslest_av_gpio[i],LCD_DATA_OUT0_IDX+i,false,false);
 }
 gdma_channel_alloc_config_t dma_config={.direction=GDMA_CHANNEL_DIRECTION_TX,.flags={.isr_cache_safe=1}};
 AV_CHECK(gdma_new_ahb_channel(&dma_config,&e->dma));
 AV_CHECK(gdma_connect(e->dma,GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD,0)));e->connected=true;
 gdma_strategy_config_t strategy={.owner_check=false,.auto_update_desc=false,.eof_till_data_popped=true};
 AV_CHECK(gdma_apply_strategy(e->dma,&strategy));
 gdma_transfer_config_t transfer={.max_data_burst_size=64,.access_ext_mem=true};AV_CHECK(gdma_config_transfer(e->dma,&transfer));
 size_t internal_align=0,external_align=0;AV_CHECK(gdma_get_alignment_constraints(e->dma,&internal_align,&external_align));
 size_t alignment=external_align>internal_align?external_align:internal_align;if(alignment<64)alignment=64;
 size_t chunk=(4095/alignment)*alignment;
 if(!chunk){result=ESP_ERR_INVALID_SIZE;goto fail;}
 size_t nodes=(AV_FIELD_BYTES+chunk-1)/chunk,allocated=(AV_FIELD_BYTES+alignment-1)/alignment*alignment;
 for(int i=0;i<3;i++){
  e->wave[i]=heap_caps_aligned_alloc(alignment,allocated,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!e->wave[i]){result=ESP_ERR_NO_MEM;goto fail;}
  memset(e->wave[i],AV_BLANK,allocated);av_signal_prime(e->wave[i],false);
  AV_CHECK(esp_cache_msync(e->wave[i],allocated,ESP_CACHE_MSYNC_FLAG_DIR_C2M));
  gdma_link_list_config_t list={.num_items=nodes,.item_alignment=4,.flags={.check_owner=false}};
  AV_CHECK(gdma_new_link_list(&list,&e->links[i]));
  gdma_buffer_mount_config_t mount={.buffer=e->wave[i],.buffer_alignment=alignment,.length=AV_FIELD_BYTES,.flags={.mark_eof=true,.mark_final=false}};
  AV_CHECK(gdma_link_mount_buffers(e->links[i],0,&mount,1,NULL));
  dma_descriptor_t *tail=(void*)gdma_link_get_head_addr(e->links[i]);
  for(size_t n=1;n<nodes;n++)tail=tail->next;
  e->tail[i]=(intptr_t)tail;
 }
 gdma_tx_event_callbacks_t callbacks={.on_trans_eof=field_eof,.on_descr_err=descriptor_error};
 AV_CHECK(gdma_register_tx_event_callbacks(e->dma,&callbacks,e));
#if CONFIG_PM_ENABLE
 AV_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP,0,"freslest_av",&e->pm));
 AV_CHECK(esp_pm_lock_acquire(e->pm));e->pm_held=true;
#endif
 AV_CHECK(gdma_start(e->dma,gdma_link_get_head_addr(e->links[0])));
 esp_rom_delay_us(4);lcd_ll_start(e->hal.dev);e->stats.running=true;*out=e;
 ESP_LOGI("freslest_av","Engine %s: raw i80/GDMA, %u samples x %u lines; %u Hz requested, %lu Hz divider prediction; 3 immutable field rings",
  FRESLEST_AV_VERSION,AV_LINE,AV_LINES,AV_SAMPLE_HZ,(unsigned long)e->stats.sample_hz);
 return ESP_OK;
fail:
 ESP_LOGE("freslest_av","Engine initialization failed: %s",esp_err_to_name(result));freslest_av_close(e);return result;
#undef AV_CHECK
}
int freslest_av_acquire(freslest_av_handle_t e,uint8_t **wave){
 if(!e||!wave)return -1;
 *wave=NULL;
 portENTER_CRITICAL(&e->mux);int slot=av_ownership_acquire(&e->ownership);portEXIT_CRITICAL(&e->mux);
 if(slot>=0)*wave=e->wave[slot];
 return slot;
}
bool freslest_av_publish(freslest_av_handle_t e,int slot){
 if(!e||slot<0||slot>=3)return false;
 portENTER_CRITICAL(&e->mux);bool ready=e->ownership.writing==slot&&e->ownership.pending<0;portEXIT_CRITICAL(&e->mux);
 if(!ready)return false;
 // Flush a COMPLETE free field before exposing it to DMA. Even a delayed
 // pointer prefetch can only select the old complete field or the new one.
 if(esp_cache_msync(e->wave[slot],AV_FIELD_BYTES,ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_UNALIGNED)!=ESP_OK)return false;
 if(gdma_link_concat(e->links[slot],-1,e->links[slot],0)!=ESP_OK)return false;
 portENTER_CRITICAL(&e->mux);int old=e->ownership.active;ready=av_ownership_queue(&e->ownership,slot);portEXIT_CRITICAL(&e->mux);
 if(!ready)return false;
 __sync_synchronize();
 // Both lists are live, immutable and internal-RAM linked. The only change
 // is an aligned next pointer at the old ring's field boundary.
 if(gdma_link_concat(e->links[old],-1,e->links[slot],0)!=ESP_OK){
  portENTER_CRITICAL(&e->mux);e->stats.descriptor_errors++;portEXIT_CRITICAL(&e->mux);return false;
 }
 return true;
}
void freslest_av_cancel(freslest_av_handle_t e,int slot){if(e){portENTER_CRITICAL(&e->mux);av_ownership_cancel(&e->ownership,slot);portEXIT_CRITICAL(&e->mux);}}
void freslest_av_stats(freslest_av_handle_t e,freslest_av_stats_t *s){
 if(!s)return;
 memset(s,0,sizeof(*s));
 if(!e)return;
 portENTER_CRITICAL(&e->mux);*s=e->stats;s->active=e->ownership.active;s->pending=e->ownership.pending;s->writing=e->ownership.writing;s->swaps=e->ownership.swaps;portEXIT_CRITICAL(&e->mux);
 s->running=s->running&&e->hal.dev->lcd_user.lcd_start;
}
bool freslest_av_healthy(freslest_av_handle_t e){
 freslest_av_stats_t s;freslest_av_stats(e,&s);
 return s.running&&s.fields>2&&s.swaps>0&&!s.descriptor_errors&&!s.unknown_eof&&
  (uint32_t)((uint32_t)esp_timer_get_time()-s.last_field_us)<500000;
}
