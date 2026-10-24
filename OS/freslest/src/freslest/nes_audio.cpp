#include "nes_audio.h"
#include "nes_pcm_queue.h"
#include "freslest_port.h"
#include <driver/ledc.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdio>

// BL uses Arduino channel 1 / timer 0. NES owns channel 4 / timer 2 only.
static constexpr auto channel=LEDC_CHANNEL_4;
static constexpr auto pwmTimer=LEDC_TIMER_2;
static constexpr unsigned center=512;
static constexpr int outputPin=21;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static NesPcmQueue samples;
static esp_timer_handle_t timer=nullptr;
static bool running=false,inCallback=false,pwmConfigured=false;
static uint32_t generation=0;
static unsigned gain=180,liveGain=0;
static int32_t lastPcm=0;
static uint32_t hardwareErrors=0;

static esp_err_t writeDuty(unsigned duty) {
  esp_err_t err=ledc_set_duty(LEDC_LOW_SPEED_MODE,channel,duty);
  return err==ESP_OK?ledc_update_duty(LEDC_LOW_SPEED_MODE,channel):err;
}
static void tick(void* context) {
  portENTER_CRITICAL(&lock);
  if(!running || (uintptr_t)context!=generation) {portEXIT_CRITICAL(&lock);return;}
  inCallback=true;
  int16_t value;
  if(samples.pop(value))lastPcm=value;
  else if(lastPcm>0)lastPcm=std::max<int32_t>(0,lastPcm-1024);
  else if(lastPcm<0)lastPcm=std::min<int32_t>(0,lastPcm+1024);
  if(gain==0)liveGain=0;
  else if(liveGain<gain)liveGain=std::min(gain,liveGain+8);
  else if(liveGain>gain)liveGain=std::max(gain,liveGain-8);
  int32_t scaled=lastPcm*(int32_t)liveGain/255;
  unsigned duty=(unsigned)std::clamp<int32_t>(center+scaled/64,0,1023);
  portEXIT_CRITICAL(&lock);
  // ESP_TIMER_TASK context: driver locking is permitted. No UART or allocations here.
  esp_err_t err=writeDuty(duty);
  portENTER_CRITICAL(&lock);
  if(err!=ESP_OK)++hardwareErrors;
  inCallback=false;
  portEXIT_CRITICAL(&lock);
}
void nesAudioStop() {
  // Gate even a callback that was already dispatched before esp_timer_stop().
  portENTER_CRITICAL(&lock);running=false;++generation;portEXIT_CRITICAL(&lock);
  if(timer) {esp_timer_stop(timer);esp_timer_delete(timer);timer=nullptr;}
  for(;;) {
    portENTER_CRITICAL(&lock);bool busy=inCallback;portEXIT_CRITICAL(&lock);
    if(!busy)break;
    vTaskDelay(1);
  }
  // All callback writes have finished. Keep the analogue output at its quiet midpoint.
  if(pwmConfigured)writeDuty(center);
  portENTER_CRITICAL(&lock);samples.reset();lastPcm=0;liveGain=0;portEXIT_CRITICAL(&lock);
}
bool nesAudioStart() {
  if(timer)nesAudioStop();
  ledc_timer_config_t config={};
  config.speed_mode=LEDC_LOW_SPEED_MODE;config.duty_resolution=LEDC_TIMER_10_BIT;
  config.timer_num=pwmTimer;config.freq_hz=31250;config.clk_cfg=LEDC_USE_XTAL_CLK; // matches Arduino backlight global clock source
  ledc_channel_config_t output={};
  output.gpio_num=outputPin;output.speed_mode=LEDC_LOW_SPEED_MODE;output.channel=channel;
  output.intr_type=LEDC_INTR_DISABLE;output.timer_sel=pwmTimer;output.duty=center;
  esp_err_t err=ledc_timer_config(&config);
  if(err==ESP_OK)err=ledc_channel_config(&output);
  if(err==ESP_OK) {pwmConfigured=true;err=writeDuty(center);}
  if(err!=ESP_OK) {printf("NES_AUDIO: PWM init failed (%d), silent video mode\n",(int)err);return false;}
  esp_timer_create_args_t cb={};
  portENTER_CRITICAL(&lock);uintptr_t token=++generation;portEXIT_CRITICAL(&lock);
  cb.callback=tick;cb.arg=(void*)token;cb.dispatch_method=ESP_TIMER_TASK;cb.name="nes_pcm";cb.skip_unhandled_events=true;
  err=esp_timer_create(&cb,&timer);
  if(err!=ESP_OK) {timer=nullptr;printf("NES_AUDIO: timer create failed (%d)\n",(int)err);return false;}
  portENTER_CRITICAL(&lock);
  samples.reset();lastPcm=0;liveGain=0;hardwareErrors=0;running=true;
  portEXIT_CRITICAL(&lock);
  err=esp_timer_start_periodic(timer,1000000/FRESLEST_NES_SAMPLE_HZ);
  if(err!=ESP_OK) {printf("NES_AUDIO: timer start failed (%d)\n",(int)err);nesAudioStop();return false;}
  printf("NES_AUDIO: GPIO%d %d Hz PCM, channel4/timer2; volume %u/255\n",outputPin,FRESLEST_NES_SAMPLE_HZ,gain);
  return true;
}
void nesAudioSetVolume(bool enabled,unsigned volume) {
  portENTER_CRITICAL(&lock);gain=enabled?std::min(volume,255u):0;portEXIT_CRITICAL(&lock);
}
void nesAudioSubmit(const int16_t* pcm,size_t count) {
  if(!pcm)return;
  // At most 32 samples per short lock; the consumer never waits for a whole video frame.
  while(count) {
    size_t chunk=std::min(count,(size_t)32);
    portENTER_CRITICAL(&lock);
    if(running)samples.push(pcm,chunk);
    portEXIT_CRITICAL(&lock);
    pcm+=chunk;count-=chunk;
  }
}
NesAudioStats nesAudioStats() {
  portENTER_CRITICAL(&lock);
  NesAudioStats result={running,samples.count(),samples.dropped(),samples.underruns(),hardwareErrors};
  portEXIT_CRITICAL(&lock);return result;
}
