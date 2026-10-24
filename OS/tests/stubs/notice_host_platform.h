#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>

using esp_err_t=int;
using nvs_handle_t=unsigned;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NVS_NOT_FOUND=1,ESP_ERR_NVS_INVALID_LENGTH=2,NVS_READWRITE=0;
struct esp_app_desc_t {char version[32];};
inline esp_app_desc_t testApp={"6.00"};
inline const esp_app_desc_t* esp_app_get_description(){return &testApp;}
inline std::string testPersistedVersion,testPendingVersion;
inline unsigned testCommits=0,testOpens=0,testCloses=0;
inline bool testNvsOpenFail=false,testNvsCommitFail=false;
inline esp_err_t nvs_open(const char* name,int,nvs_handle_t* handle){
  assert(!strcmp(name,"fl_notice"));++testOpens;
  if(testNvsOpenFail)return ESP_FAIL;
  testPendingVersion=testPersistedVersion;*handle=1;return ESP_OK;
}
inline esp_err_t nvs_get_str(nvs_handle_t,const char* key,char* value,size_t* length){
  assert(!strcmp(key,"version"));
  if(testPersistedVersion.empty())return ESP_ERR_NVS_NOT_FOUND;
  if(testPersistedVersion.size()+1>*length)return ESP_ERR_NVS_INVALID_LENGTH;
  memcpy(value,testPersistedVersion.c_str(),testPersistedVersion.size()+1);*length=testPersistedVersion.size()+1;return ESP_OK;
}
inline esp_err_t nvs_set_str(nvs_handle_t,const char* key,const char* value){assert(!strcmp(key,"version"));testPendingVersion=value;return ESP_OK;}
inline esp_err_t nvs_commit(nvs_handle_t){++testCommits;if(testNvsCommitFail)return ESP_FAIL;testPersistedVersion=testPendingVersion;return ESP_OK;}
inline void nvs_close(nvs_handle_t){++testCloses;}
