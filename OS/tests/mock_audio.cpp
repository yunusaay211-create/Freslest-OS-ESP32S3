#include "mock_audio.h"
#include <cassert>
#include <mutex>
#include <condition_variable>
struct esp_timer {esp_timer_create_args_t callback;bool active;};
static esp_timer storage;
static bool exists=false;
static uint16_t duty=512;
std::vector<uint16_t> mockDuties;
bool mockFailPwm=false,mockFailCreate=false,mockFailStart=false,mockFailWrite=false;
unsigned mockCreated=0,mockDeleted=0;
static std::mutex writeLock;
static std::condition_variable writeCV;
static bool blocked=false,writeEntered=false;
void mockBlockWrite(bool value) {std::lock_guard<std::mutex> l(writeLock);blocked=value;writeEntered=false;writeCV.notify_all();}
void mockWaitForBlockedWrite() {std::unique_lock<std::mutex> l(writeLock);assert(writeCV.wait_for(l,std::chrono::seconds(2),[]{return writeEntered;}));}
esp_err_t ledc_timer_config(const ledc_timer_config_t* c) {
 assert(c->timer_num==LEDC_TIMER_2&&c->freq_hz==31250&&c->duty_resolution==10&&c->clk_cfg==LEDC_USE_XTAL_CLK);return mockFailPwm?ESP_FAIL:ESP_OK;
}
esp_err_t ledc_channel_config(const ledc_channel_config_t* c) {assert(c->gpio_num==21&&c->channel==4&&c->timer_sel==2&&c->duty==512);return ESP_OK;}
esp_err_t ledc_set_duty(int,int channel,uint32_t d) {
 assert(channel==4&&d<=1023);
 std::unique_lock<std::mutex> l(writeLock);
 if(blocked){writeEntered=true;writeCV.notify_all();writeCV.wait(l,[]{return !blocked;});}
 duty=d;return mockFailWrite?ESP_FAIL:ESP_OK;
}
esp_err_t ledc_update_duty(int,int channel) {assert(channel==4);mockDuties.push_back(duty);return ESP_OK;}
esp_err_t esp_timer_create(const esp_timer_create_args_t* c,esp_timer_handle_t* h) {
 assert(!exists&&c->skip_unhandled_events&&c->dispatch_method==ESP_TIMER_TASK);
 if(mockFailCreate)return ESP_FAIL;
 exists=true;storage={*c,false};*h=&storage;++mockCreated;return ESP_OK;
}
esp_err_t esp_timer_start_periodic(esp_timer_handle_t h,uint64_t us) {assert(exists&&h==&storage&&us==125);if(mockFailStart)return ESP_FAIL;h->active=true;return ESP_OK;}
esp_err_t esp_timer_stop(esp_timer_handle_t h) {assert(exists&&h==&storage);h->active=false;return ESP_OK;}
esp_err_t esp_timer_delete(esp_timer_handle_t h) {assert(exists&&h==&storage&&!h->active);exists=false;++mockDeleted;return ESP_OK;}
void mockAudioTick(unsigned n) {for(unsigned i=0;i<n;i++)if(storage.active){auto cb=storage.callback;cb.callback(cb.arg);}}
esp_timer_create_args_t mockCallback() {return storage.callback;}
