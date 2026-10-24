#pragma once
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <deque>
#include <functional>
#include <algorithm>
#include <freslest_hid.h>
#include <freslest_hid_map.h>
using esp_err_t=int;using esp_event_base_t=const char*;using nvs_handle_t=int;using portMUX_TYPE=int;
constexpr int ESP_OK=0,portMUX_INITIALIZER_UNLOCKED=0,pdTRUE=1,pdPASS=1,NVS_READONLY=0,NVS_READWRITE=1;
inline int critical=0;inline uint32_t fakeNow=100;inline bool nvsFail=false;inline std::vector<uint8_t> nvsBytes;
constexpr int MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2,MALLOC_CAP_SPIRAM=4;
inline size_t internalFree=131072,internalLargest=65536;
inline unsigned sdkStarts=0;
inline size_t heap_caps_get_free_size(int){return internalFree;}
inline size_t heap_caps_get_largest_free_block(int){return internalLargest;}
#define portENTER_CRITICAL(x) (++critical)
#define portEXIT_CRITICAL(x) (--critical)
#define pdMS_TO_TICKS(x) (x)
inline uint32_t millis(){return fakeNow;}inline void vTaskDelay(unsigned n){fakeNow+=n;}
inline size_t strlcpy(char*d,const char*s,size_t n){size_t k=strlen(s);if(n){memcpy(d,s,std::min(k,n-1));d[std::min(k,n-1)]=0;}return k;}
struct Debug{template<class...A>void printf(const char*,A...){} };inline Debug Serial;
inline const char* esp_err_to_name(int){return "mock error";}
struct Queue{unsigned cap,size;std::deque<std::vector<uint8_t>> data;};using QueueHandle_t=Queue*;
inline Queue* xQueueCreate(unsigned c,unsigned s){return new Queue{c,s,{}};}inline void vQueueDelete(Queue*q){delete q;}
inline void xQueueReset(Queue*q){q->data.clear();}
inline int xQueueSend(Queue*q,const void*p,unsigned){if(q->data.size()==q->cap)return 0;q->data.emplace_back((uint8_t*)p,(uint8_t*)p+q->size);return 1;}
inline int xQueueReceive(Queue*q,void*p,unsigned){if(q->data.empty())return 0;memcpy(p,q->data.front().data(),q->size);q->data.pop_front();return 1;}
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void*,int){return 1;}
inline int nvs_open(const char*,int,int*n){assert(!critical);*n=1;return nvsFail?-1:0;}
inline int nvs_get_blob(int,const char*,void*p,size_t*n){if(nvsBytes.size()!=*n)return -1;memcpy(p,nvsBytes.data(),*n);return 0;}
inline int nvs_set_blob(int,const char*,const void*p,size_t n){nvsBytes.assign((uint8_t*)p,(uint8_t*)p+n);return nvsFail?-1:0;}
inline int nvs_commit(int){return nvsFail?-1:0;}inline void nvs_close(int){}
enum {ESP_BT_STATUS_SUCCESS,ESP_BT_CONTROLLER_STATUS_IDLE,ESP_BT_CONTROLLER_STATUS_ENABLED,ESP_BLUEDROID_STATUS_UNINITIALIZED,ESP_BLUEDROID_STATUS_ENABLED,ESP_BT_MODE_BLE,ESP_LE_AUTH_BOND,ESP_IO_CAP_OUT,ESP_BLE_ENC_KEY_MASK=1,ESP_BLE_ID_KEY_MASK=2,ESP_BLE_SM_AUTHEN_REQ_MODE,ESP_BLE_SM_IOCAP_MODE,ESP_BLE_SM_SET_INIT_KEY,ESP_BLE_SM_SET_RSP_KEY,ESP_BLE_SM_MAX_KEY_SIZE,ESP_HID_TRANSPORT_BLE,BLE_ADDR_TYPE_RPA_PUBLIC,BLE_ADDR_TYPE_PUBLIC,BLE_SCAN_TYPE_ACTIVE,BLE_SCAN_FILTER_ALLOW_ALL,BLE_SCAN_DUPLICATE_DISABLE};
struct esp_bt_controller_config_t{};
#define BT_CONTROLLER_INIT_CONFIG_DEFAULT() esp_bt_controller_config_t{}
inline int esp_bt_controller_get_status(){return ESP_BT_CONTROLLER_STATUS_ENABLED;}inline int esp_bt_controller_init(esp_bt_controller_config_t*){return 0;}inline int esp_bt_controller_enable(int){return 0;}
inline int bluedroidState=ESP_BLUEDROID_STATUS_ENABLED;inline size_t freeAfterInit=131072;inline unsigned bluedroidEnables=0;
inline int esp_bluedroid_get_status(){return bluedroidState;}inline int esp_bluedroid_init(){internalFree=freeAfterInit;return 0;}inline int esp_bluedroid_enable(){++bluedroidEnables;bluedroidState=ESP_BLUEDROID_STATUS_ENABLED;return 0;}
struct esp_ble_bond_dev_t{uint8_t bd_addr[6],bd_addr_type;};inline std::vector<esp_ble_bond_dev_t> bonds;
inline int esp_ble_get_bond_device_num(){return bonds.size();}
inline int esp_ble_get_bond_device_list(int*n,esp_ble_bond_dev_t*b){memcpy(b,bonds.data(),*n*sizeof(*b));return 0;}
inline int esp_ble_remove_bond_device(uint8_t*){assert(!critical);return 0;}
enum esp_gap_ble_cb_event_t {ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT,ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT,ESP_GAP_BLE_SCAN_START_COMPLETE_EVT,ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT,ESP_GAP_BLE_SCAN_RESULT_EVT,ESP_GAP_BLE_SEC_REQ_EVT,ESP_GAP_BLE_PASSKEY_NOTIF_EVT,ESP_GAP_BLE_NC_REQ_EVT,ESP_GAP_BLE_PASSKEY_REQ_EVT,ESP_GAP_BLE_AUTH_CMPL_EVT};
enum {ESP_GAP_SEARCH_INQ_CMPL_EVT,ESP_GAP_SEARCH_INQ_RES_EVT};
struct esp_ble_gap_cb_param_t {
 struct{int status;}local_privacy_cmpl,scan_param_cmpl,scan_start_cmpl;
 struct{int search_evt;uint8_t bda[6],ble_addr_type,adv_data_len,scan_rsp_len,ble_adv[62];}scan_rst;
 struct{struct{uint8_t bd_addr[6];}ble_req;struct{uint8_t bd_addr[6];unsigned passkey;}key_notif;struct{uint8_t bd_addr[6];bool success;}auth_cmpl;}ble_security;
};
using gap_cb=void(*)(esp_gap_ble_cb_event_t,esp_ble_gap_cb_param_t*);inline gap_cb gapCallback=nullptr;
struct esp_ble_scan_params_t{int scan_type,own_addr_type,scan_filter_policy,scan_interval,scan_window,scan_duplicate;};
struct esp_ble_conn_update_params_t{uint8_t bda[6];int min_int,max_int,latency,timeout;};
inline unsigned scans=0,opens=0;inline std::vector<std::vector<uint8_t>> disconnected;
inline int esp_ble_gap_register_callback(gap_cb cb){++sdkStarts;gapCallback=cb;return 0;}
inline int esp_ble_gap_start_scanning(unsigned){assert(!critical);++scans;if(gapCallback){esp_ble_gap_cb_param_t p{};gapCallback(ESP_GAP_BLE_SCAN_START_COMPLETE_EVT,&p);}return 0;}
inline int esp_ble_gap_stop_scanning(){assert(!critical);if(gapCallback){esp_ble_gap_cb_param_t p{};gapCallback(ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT,&p);}return 0;}
inline int esp_ble_gap_set_scan_params(esp_ble_scan_params_t*){assert(!critical);if(gapCallback){esp_ble_gap_cb_param_t p{};gapCallback(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT,&p);}return 0;}
inline int esp_ble_gap_config_local_privacy(bool){if(gapCallback){esp_ble_gap_cb_param_t p{};gapCallback(ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT,&p);}return 0;}
inline int esp_ble_gap_disconnect(uint8_t*b){assert(!critical);disconnected.emplace_back(b,b+6);return 0;}
inline int esp_ble_gap_update_conn_params(esp_ble_conn_update_params_t*p){assert(!critical&&p->timeout==600);return 0;}
inline int esp_ble_gap_security_rsp(uint8_t*,bool){return 0;}inline int esp_ble_passkey_reply(uint8_t*,bool,int){return 0;}inline int esp_ble_confirm_reply(uint8_t*,bool){return 0;}inline int esp_ble_gap_set_security_param(int,void*,int){return 0;}
enum {ESP_HIDH_OPEN_EVENT,ESP_HIDH_INPUT_EVENT,ESP_HIDH_CLOSE_EVENT};
struct esp_hid_raw_report_map_t {const uint8_t*data;uint16_t len;};
struct esp_hidh_dev_t {uint8_t address[6];esp_hid_raw_report_map_t maps[2];unsigned count=1;bool freed=false;};
struct esp_hidh_event_data_t{struct{esp_hidh_dev_t*dev;int status;}open;struct{esp_hidh_dev_t*dev;unsigned map_index,report_id,length;uint8_t*data;}input;struct{esp_hidh_dev_t*dev;int reason;}close;};
struct esp_hidh_config_t{void(*callback)(void*,esp_event_base_t,int32_t,void*);unsigned event_stack_size;};
inline int esp_hidh_init(esp_hidh_config_t*){return 0;}inline void esp_hidh_gattc_event_handler(){}inline int esp_ble_gattc_register_callback(void(*)()){return 0;}
inline std::function<esp_hidh_dev_t*(uint8_t*,int)> openHook;
inline esp_hidh_dev_t* esp_hidh_dev_open(uint8_t*b,int,int type){assert(!critical);++opens;return openHook?openHook(b,type):nullptr;}
inline const uint8_t* esp_hidh_dev_bda_get(esp_hidh_dev_t*d){assert(!critical&&!d->freed);return d->address;}
inline const char* esp_hidh_dev_name_get(esp_hidh_dev_t*d){assert(!critical&&!d->freed);return "Test HID";}
inline int esp_hidh_dev_report_maps_get(esp_hidh_dev_t*d,size_t*n,esp_hid_raw_report_map_t**m){assert(!critical&&!d->freed);*n=d->count;*m=d->maps;return 0;}
