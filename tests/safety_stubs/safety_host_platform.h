#pragma once
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <initializer_list>
#define RTC_NOINIT_ATTR
#define portMUX_INITIALIZER_UNLOCKED 0
using portMUX_TYPE=int;
#define portENTER_CRITICAL(x) (void)(x)
#define portEXIT_CRITICAL(x) (void)(x)
using vprintf_like_t=int(*)(const char*,va_list);
using esp_err_t=int;using nvs_handle_t=unsigned;using gpio_num_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,NVS_READONLY=0,NVS_READWRITE=1,INPUT_PULLUP=2,LOW=0,HIGH=1;
constexpr int ESP_ERR_NOT_FOUND=0x105;
constexpr int BTN_UP=4,BTN_DOWN=5,BTN_LEFT=6,BTN_RIGHT=7,BTN_OK=15,BTN_BACK=16,BTN_FIRE=3;
constexpr int ESP_RST_POWERON=1,ESP_RST_EXT=2,ESP_RST_SW=3,ESP_RST_PANIC=4,ESP_RST_INT_WDT=5,ESP_RST_TASK_WDT=6,ESP_RST_WDT=7,ESP_RST_DEEPSLEEP=8,ESP_RST_BROWNOUT=9;
constexpr int MALLOC_CAP_INTERNAL=1,RTC_GPIO_MODE_INPUT_ONLY=1,ESP_EXT1_WAKEUP_ANY_LOW=0;
constexpr int ESP_PARTITION_TYPE_APP=0,ESP_PARTITION_SUBTYPE_APP_OTA_0=16,ESP_PARTITION_SUBTYPE_APP_OTA_1=17,ESP_IMAGE_VERIFY=0;
constexpr unsigned FRESLEST_OS_SLOT_SIZE=0x1b0000;
using esp_ota_img_states_t=int;constexpr int ESP_OTA_IMG_VALID=2,ESP_OTA_IMG_UNDEFINED=-1;
struct esp_partition_t { unsigned address,size;int subtype; };
struct esp_partition_pos_t {unsigned offset,size;};struct esp_image_metadata_t{};
struct esp_app_desc_t {char version[32],project_name[32];};
inline uint32_t fakeNow=100;inline int resetReason=ESP_RST_POWERON;
inline std::map<int,int> pinLevels;inline std::map<std::string,std::vector<uint8_t>> nv;
inline std::map<unsigned,std::string> handles;inline unsigned nextHandle=1,saveCount=0;
inline bool nvsOk=true,commitOk=true;
inline std::vector<std::string> drawn;inline std::vector<int> script;inline size_t scriptAt=0;inline int scriptedKey=-1;
inline std::function<void()> onPoll;
inline unsigned radioCalls=0,restarts=0,sleeps=0,settingsWrites=0,otaSelects=0,rtcDeinits=0;
inline int wakeResult=ESP_OK,btStopResult=ESP_OK,wifiStopResult=ESP_OK;
inline uint64_t wakeMask=0;inline int wakeMode=-1,btHost=0,btController=0;
inline uint32_t lastHid=0;
inline bool deviceRole=false,storageSleepOk=true;
inline bool available=true,correctProject=true,hashOk=true,verifyOk=true,portalActive=false,browserBusy=false;
inline int previousState=ESP_OTA_IMG_VALID;inline esp_err_t selectResult=ESP_OK,otaStateResult=ESP_OK;
inline esp_partition_t running{0x100000,0x1b0000,16},previousPart{0x2b0000,0x1b0000,17};
inline void pinMode(int,int){}
inline int digitalRead(int p){return pinLevels.count(p)?pinLevels[p]:HIGH;}
inline uint32_t millis(){return fakeNow;}
inline void delay(unsigned n){fakeNow+=n;}
inline size_t strlcpy(char*d,const char*s,size_t n){size_t l=strlen(s);if(n){size_t m=std::min(l,n-1);memcpy(d,s,m);d[m]=0;}return l;}
struct Button {
 int pin;bool stable=HIGH,lastRead=HIGH,pressedEdge=false;uint32_t lastChange=0;
 void update(uint32_t){if(pin==BTN_UP){if(onPoll)onPoll();if(scriptAt>=script.size())throw std::runtime_error("button script exhausted");scriptedKey=script[scriptAt++];}pressedEdge=pin==scriptedKey;stable=digitalRead(pin);}
 bool down()const{return stable==LOW;}bool pressed()const{return pressedEdge;}
};
inline Button bUp{BTN_UP},bDown{BTN_DOWN},bLeft{BTN_LEFT},bRight{BTN_RIGHT},bOk{BTN_OK},bBack{BTN_BACK},bFire{BTN_FIRE};
inline void inputUpdate(uint32_t now){for(Button*b:{&bUp,&bDown,&bLeft,&bRight,&bOk,&bBack,&bFire})b->update(now);}
struct Theme{unsigned bg=0,panel=1,text=2,dim=3,selbg=4;};inline Theme&TH(){static Theme t;return t;}
constexpr int TFT_RED=0xf800;
struct Screen {
 void fillScreen(unsigned){} void fillRect(int,int,int,int,unsigned){} void fillRoundRect(int,int,int,int,int,unsigned){}
 void setTextColor(unsigned,unsigned){} void setTextSize(int){} void setCursor(int,int){}
 void print(const char*s){drawn.emplace_back(s);} template<class...T>void printf(const char*f,T...a){char s[256];snprintf(s,sizeof(s),f,a...);print(s);}void endWrite(){}
 int height(){return 320;}
};inline Screen tft;
struct SerialPort{void println(const char*){}void flush(){}};inline SerialPort Serial;
struct Restart{};struct Slept{};
struct Esp{void restart(){++restarts;throw Restart{};}};inline Esp ESP;
struct Fs{void end(){}};inline Fs LittleFS;
inline bool wifiEnabled=true,btEnabled=true;
inline void gfxMirrorTick(){} inline bool screenMirrorActive(){return false;} inline void gfxPresent(){} inline void backlightSet(int){}inline void audioStop(){}inline void audioUpdate(uint32_t){}
inline void settingsSave(){++settingsWrites;}
inline unsigned esp_reset_reason(){return resetReason;}
inline esp_err_t nvs_flash_init(){return nvsOk?ESP_OK:ESP_FAIL;}
inline esp_err_t nvs_open(const char*ns,int mode,nvs_handle_t*n){
 if(!nvsOk)return ESP_FAIL;
 if(mode==NVS_READONLY){bool exists=false;for(auto&p:nv)exists|=p.first.rfind(std::string(ns)+"/",0)==0;if(!exists)return ESP_FAIL;}
 *n=nextHandle++;handles[*n]=ns;return ESP_OK;
}
inline void nvs_close(nvs_handle_t n){handles.erase(n);}
inline esp_err_t nvs_set_blob(nvs_handle_t n,const char*k,const void*p,size_t l){if(!commitOk)return ESP_FAIL;auto*b=(const uint8_t*)p;nv[handles[n]+"/"+k]={b,b+l};return ESP_OK;}
inline esp_err_t nvs_get_blob(nvs_handle_t n,const char*k,void*p,size_t*l){auto it=nv.find(handles[n]+"/"+k);if(it==nv.end()||*l<it->second.size())return ESP_FAIL;*l=it->second.size();memcpy(p,it->second.data(),*l);return ESP_OK;}
inline esp_err_t nvs_set_u8(nvs_handle_t n,const char*k,uint8_t v){return nvs_set_blob(n,k,&v,1);}
inline esp_err_t nvs_get_u8(nvs_handle_t n,const char*k,uint8_t*v){size_t size=1;return nvs_get_blob(n,k,v,&size);}
inline esp_err_t nvs_commit(nvs_handle_t){++saveCount;return commitOk?ESP_OK:ESP_FAIL;}
inline int defaultLogger(const char*,va_list){return 0;}
inline vprintf_like_t logger=defaultLogger;
inline vprintf_like_t esp_log_set_vprintf(vprintf_like_t f){auto p=logger;logger=f;return p;}
inline esp_err_t esp_register_shutdown_handler(void(*)()){return ESP_OK;}
inline size_t heap_caps_get_free_size(int){return 64000;}inline size_t heap_caps_get_largest_free_block(int){return 32000;}
inline const esp_app_desc_t*esp_app_get_description(){static esp_app_desc_t d{"6.13","freslest_os_s3"};return &d;}
inline const esp_partition_t*esp_ota_get_running_partition(){return &running;}
inline const esp_partition_t*esp_partition_find_first(int,int,const char*){return available?&previousPart:nullptr;}
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t*,esp_ota_img_states_t*s){*s=previousState;return otaStateResult;}
inline esp_err_t esp_ota_get_partition_description(const esp_partition_t*,esp_app_desc_t*d){strlcpy(d->version,"6.12",32);strlcpy(d->project_name,correctProject?"freslest_os_s3":"freslest_doom_s3",32);return ESP_OK;}
inline esp_err_t esp_partition_get_sha256(const esp_partition_t*,uint8_t*){return hashOk?ESP_OK:ESP_FAIL;}
inline esp_err_t esp_image_verify(int,const esp_partition_pos_t*,esp_image_metadata_t*){return verifyOk?ESP_OK:ESP_FAIL;}
inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t*p){assert(p==&previousPart&&p->address!=0x20000);++otaSelects;return selectResult;}
inline const char*esp_err_to_name(int e){return e?"ESP_FAIL":"ESP_OK";}
inline void rtc_gpio_init(int){}inline void rtc_gpio_set_direction(int,int){}inline void rtc_gpio_pullup_en(int){}inline void rtc_gpio_pulldown_dis(int){}inline void rtc_gpio_hold_dis(int){}inline void rtc_gpio_deinit(int){++rtcDeinits;}
inline esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t m,int l){wakeMask=m;wakeMode=l;return wakeResult;}
inline void esp_deep_sleep_start(){++sleeps;throw Slept{};}
constexpr int ESP_BLUEDROID_STATUS_ENABLED=2,ESP_BT_CONTROLLER_STATUS_ENABLED=2,WIFI_OFF=0;
inline int esp_bluedroid_get_status(){return btHost;}inline esp_err_t esp_bluedroid_disable(){++radioCalls;btHost=0;return btStopResult;}
inline int esp_bt_controller_get_status(){return btController;}inline esp_err_t esp_bt_controller_disable(){++radioCalls;btController=0;return btStopResult;}
struct Wifi{bool mode(int m){assert(m==WIFI_OFF);++radioCalls;return wifiStopResult==ESP_OK;}};inline Wifi WiFi;
struct BleInfo{bool enabled=false;char status[76]="Bluetooth is off.";};inline BleInfo ble;
inline BleInfo freslestBleInfo(){return ble;}inline void freslestBleStop(){++radioCalls;ble.enabled=false;strlcpy(ble.status,"Bluetooth is off.",76);}
inline uint32_t usbInputLastActivity(){return lastHid;}
inline bool usbInputDeviceMode(){return deviceRole;}
inline bool usbStoragePrepareSleep(){return storageSleepOk;}
inline bool wifiPortalActive(){return portalActive;}
struct BrowserStatus{bool busy;};inline BrowserStatus nativeBrowserStatus(){return {browserBusy};}
enum class UsbStorageState{Disabled,Absent,Mounting,Ready,Error,Ejected};
struct DiskInfo {UsbStorageState state=UsbStorageState::Absent;bool present(){return state!=UsbStorageState::Disabled&&state!=UsbStorageState::Absent;}};inline DiskInfo disk;
inline DiskInfo usbStorageInfo(){return disk;}
enum AppId{APP_MENU,APP_USB_TRANSFER,APP_FRESLEST_UPDATE,APP_PAINT,APP_NOTE,APP_CAMERA,APP_BROWSER,APP_NES_PLAYER};inline int app=APP_MENU;
