#include "install_notice.h"
#ifdef FRESLEST_NOTICE_HOST_TEST
#include "notice_host_platform.h"
#else
#include <esp_app_desc.h>
#include <nvs.h>
#endif
#include <cstdio>
#include <cstring>

namespace {
bool checked=false,visible=false,retryPending=false;
uint32_t shownAt=0,retryAt=0;
char message[64]={};
constexpr uint32_t DURATION=6000;

bool recordVersion(const char* version,bool& alreadySeen) {
  nvs_handle_t handle;
  if(nvs_open("fl_notice",NVS_READWRITE,&handle)!=ESP_OK)return false;
  char previous[32]={};size_t length=sizeof(previous);
  esp_err_t result=nvs_get_str(handle,"version",previous,&length);
  if(result==ESP_OK&&strcmp(previous,version)==0) {
    alreadySeen=true;nvs_close(handle);return true;
  }
  if(result!=ESP_OK&&result!=ESP_ERR_NVS_NOT_FOUND&&result!=ESP_ERR_NVS_INVALID_LENGTH) {
    nvs_close(handle);return false;
  }
  result=nvs_set_str(handle,"version",version);
  if(result==ESP_OK)result=nvs_commit(handle);
  nvs_close(handle);
  return result==ESP_OK;
}
}

const char* installNoticeText() {return visible?message:nullptr;}

bool installNoticeUpdate(uint32_t now,bool startupReady,bool headerVisible) {
  if(visible&&now-shownAt>=DURATION) {visible=false;return true;}
  if(checked||!startupReady||!headerVisible)return false;
  if(retryPending&&(int32_t)(now-retryAt)<0)return false;
  const char* version=esp_app_get_description()->version;
  if(!version[0])return false;
  bool alreadySeen=false;
  if(!recordVersion(version,alreadySeen)) {
    retryPending=true;retryAt=now+30000;return false;
  }
  // Commit before drawing: a subsequent reset must not repeat this banner.
  checked=true;retryPending=false;
  if(alreadySeen)return false;
  snprintf(message,sizeof(message),"Freslest OS %.14s installed",version);
  shownAt=now;visible=true;return true;
}
