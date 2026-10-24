// SPDX-License-Identifier: MIT
#include "power_recovery.h"
#include "safety_policy.h"
#ifdef FRESLEST_SAFETY_HOST_TEST
#include "safety_host_platform.h"
#else
#include "settings.h"
#include "apps.h"
#include "audio_manager.h"
#include "browser_native.h"
#include "wifi_manager.h"
#include "ble_advertising.h"
#include "usb_storage.h"
#include "../../gfx.h"
#include "../../screen_mirror.h"
#include "../../input.h"
#include "../../usb_input.h"
#include <LittleFS.h>
#include <esp_attr.h>
#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <driver/rtc_io.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <freslest_video.h>
#include <freslest_ota.h>
#endif
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
using freslest_safe::BootPolicy;
struct Record {
  uint32_t magic;
  BootPolicy policy;
  uint8_t stage, lastStage, lastReset;
  char error[96];
};
Record record{};
constexpr uint32_t MAGIC = 0x46525313;
// Diagnostics survive ordinary soft/watchdog resets. NVS additionally keeps
// the boot stage and last application error when RTC RAM is lost.
struct LogRing { uint32_t magic; unsigned head, count; char line[10][96]; };
RTC_NOINIT_ATTR LogRing logRing;
portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t originalLog;
freslest_safe::IdlePolicy idle;
bool storeOk = false, healthyBoot = false, inRecovery = false;
uint32_t stableAt = 0;
bool stable = false;
const char *const stageNames[] = {"Boot", "Storage", "Display", "Settings", "USB",
  "Menu", "Wi-Fi startup", "Bluetooth startup", "Ready", "Deep sleep"};
const char *stageName(unsigned i) { return i < 10 ? stageNames[i] : "Unknown"; }
const char *resetName(unsigned r) {
  switch (r) {
  case ESP_RST_POWERON: return "Power on";
  case ESP_RST_EXT: return "External reset";
  case ESP_RST_SW: return "Software reset";
  case ESP_RST_PANIC: return "Panic / exception";
  case ESP_RST_INT_WDT: return "Interrupt watchdog";
  case ESP_RST_TASK_WDT: return "Task watchdog";
  case ESP_RST_WDT: return "Watchdog";
  case ESP_RST_DEEPSLEEP: return "Deep-sleep wake";
  case ESP_RST_BROWNOUT: return "Brownout / low supply";
  default: return "Other reset";
  }
}
void logLine(const char *s) {
  portENTER_CRITICAL(&logMux);
  char *out = logRing.line[logRing.head];
  unsigned n = 0;
  // Strip terminal colour sequences and controls before putting text on TFT.
  for (unsigned i = 0; s[i] && n < 95; ++i) {
    if ((unsigned char)s[i] == 27 && s[i+1] == '[') {
      i += 2; while (s[i] && s[i] != 'm') ++i;
      if (!s[i]) break;
    } else if ((unsigned char)s[i] >= 32 && (unsigned char)s[i] < 127) out[n++] = s[i];
  }
  out[n] = 0;
  logRing.head = (logRing.head + 1) % 10;
  if (logRing.count < 10) ++logRing.count;
  portEXIT_CRITICAL(&logMux);
}
int logOutput(const char *format, va_list args) {
  char line[192]; va_list copy; va_copy(copy,args);
  vsnprintf(line,sizeof(line),format,copy); va_end(copy);
  // Keep warning/error diagnostics, not HTTP bodies, credentials or key input.
  if (strstr(line,"E (") || strstr(line,"W (")) logLine(line);
  return originalLog ? originalLog(format,args) : 0;
}
bool save() {
  nvs_handle_t n;
  if (!storeOk || nvs_open("fl_safe", NVS_READWRITE, &n) != ESP_OK) return false;
  esp_err_t e = nvs_set_blob(n,"boot",&record,sizeof(record));
  if (e == ESP_OK) e = nvs_commit(n);
  nvs_close(n);
  return e == ESP_OK;
}
void cleanExit() {
  // esp_restart can invoke this on the OTA timer task. Keep its marker in a
  // separate NVS key, avoiding concurrent access to the UI-owned boot record.
  nvs_handle_t n;
  if(storeOk && nvs_open("fl_safe",NVS_READWRITE,&n)==ESP_OK){
    if(nvs_set_u8(n,"clean",1)==ESP_OK)nvs_commit(n);
    nvs_close(n);
  }
}
void poll(uint32_t now) {
  gfxMirrorTick();
  // Shared input also accepts UART viewer commands; radios/USB host stay off.
  inputUpdate(now);
}
void releaseButtons() {
  while (digitalRead(BTN_OK)==LOW || digitalRead(BTN_BACK)==LOW ||
         digitalRead(BTN_LEFT)==LOW || digitalRead(BTN_RIGHT)==LOW) delay(10);
  for (Button *b : {&bUp,&bDown,&bLeft,&bRight,&bOk,&bBack,&bFire}) {
    b->stable=b->lastRead=digitalRead(b->pin); b->pressedEdge=false;
    b->lastChange=millis();
  }
}
void title(const char *s, const char *hint) {
  tft.fillScreen(TH().bg); tft.fillRect(0,0,480,44,TH().panel);
  tft.setTextColor(TH().text,TH().panel);tft.setTextSize(2);
  tft.setCursor(12,12);tft.print(s);
  tft.setTextSize(1);tft.setTextColor(TH().dim,TH().bg);
  tft.setCursor(18,tft.height()-18);tft.print(hint);
}
void row(int i, int selected, const char *s, bool enabled = true) {
  int y=94+i*34;
  tft.fillRoundRect(18,y,444,30,5,selected==i?TH().selbg:TH().panel);
  tft.setTextColor(enabled?TH().text:TH().dim,selected==i?TH().selbg:TH().panel);
  tft.setTextSize(1);tft.setCursor(28,y+10);tft.print(s);
}
void viewLog() {
  int offset=0;
  for (;;) {
    title("See Log","UP/DOWN Scroll   BACK Recovery   UART: i = diagnostics");
    tft.setTextColor(TH().text,TH().bg);tft.setTextSize(1);
    tft.setCursor(18,52);tft.printf("OS %s | Reset: %s",esp_app_get_description()->version,resetName(record.lastReset));
    tft.setCursor(18,68);tft.printf("Previous stage: %s | Interrupted boots: %u",stageName(record.lastStage),record.policy.failures);
    tft.setCursor(18,84);tft.printf("Internal free: %u | Largest: %u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    tft.setCursor(18,100);tft.printf("Last error: %.61s",record.error[0]?record.error:"No saved application error");
    unsigned count,head;char lines[10][96];
    portENTER_CRITICAL(&logMux);count=logRing.count;head=logRing.head;
    memcpy(lines,logRing.line,sizeof(lines));portEXIT_CRITICAL(&logMux);
    int visible=(tft.height()-150)/24;if(visible<1)visible=1;
    int maxOffset=count>(unsigned)visible?(int)count-visible:0;
    if(offset>maxOffset)offset=maxOffset;
    for(int i=0;i<visible && i+offset<(int)count;++i) {
      unsigned n=(head+10-count+i+offset)%10;
      tft.setCursor(18,122+i*24);tft.printf("%.73s",lines[n]);
      if(strlen(lines[n])>73){tft.setCursor(18,132+i*24);tft.print(lines[n]+73);}
    }
    gfxPresent();
    for (;;) {
      poll(millis());
      if(bBack.pressed()){releaseButtons();return;}
      if(bUp.pressed()){if(offset>0)--offset;break;}
      if(bDown.pressed()){if(offset<maxOffset)++offset;break;}
      delay(10);
    }
  }
}
bool previousStateEligible(const esp_partition_t *p,unsigned current) {
  esp_ota_img_states_t state=ESP_OTA_IMG_UNDEFINED;
  esp_err_t e=esp_ota_get_state_partition(p,&state);
  if(e==ESP_OK && state==ESP_OTA_IMG_VALID)return true;
  if(e!=ESP_ERR_NOT_FOUND && (e!=ESP_OK || state!=ESP_OTA_IMG_UNDEFINED))return false;
  // The first serial-installed slot has no otadata entry. Accept it only
  // when the existing v2 update journal explicitly identifies the old OS.
  struct Journal {uint32_t magic;uint8_t schema,mode,source,target,active,previous,reserved[6];} j{};
  static_assert(sizeof(j)==16,"v2 update journal layout");
  nvs_handle_t n;size_t bytes=sizeof(j);
  if(nvs_open("fl_update",NVS_READONLY,&n)!=ESP_OK)return false;
  e=nvs_get_blob(n,"journal",&j,&bytes);nvs_close(n);
  return e==ESP_OK && bytes==sizeof(j) && j.magic==0x46524f54 && j.schema==2 &&
    ((j.mode==0 && j.active==current && j.previous==(current^1u)) ||
     (j.mode==3 && j.target==current && j.source==(current^1u)));
}
const esp_partition_t *previousOs(char *version, size_t size) {
  const esp_partition_t *running=esp_ota_get_running_partition();
  if(!running || (running->address!=0x100000 && running->address!=0x2b0000))return nullptr;
  const esp_partition_t *p=esp_partition_find_first(ESP_PARTITION_TYPE_APP,
    running->address==0x100000?ESP_PARTITION_SUBTYPE_APP_OTA_1:ESP_PARTITION_SUBTYPE_APP_OTA_0,
    running->address==0x100000?"os_b":"os_a");
  esp_app_desc_t desc{};uint8_t digest[32];
  if(!p || p->size!=FRESLEST_OS_SLOT_SIZE ||
     !previousStateEligible(p,running->address==0x100000?0:1) ||
     esp_ota_get_partition_description(p,&desc)!=ESP_OK || strcmp(desc.project_name,"freslest_os_s3") ||
     esp_partition_get_sha256(p,digest)!=ESP_OK)return nullptr;
  esp_partition_pos_t pos{p->address,p->size};esp_image_metadata_t image{};
  if(esp_image_verify(ESP_IMAGE_VERIFY,&pos,&image)!=ESP_OK)return nullptr;
  strlcpy(version,desc.version,size);return p;
}
bool setRadios(bool wifi, bool bluetooth) {
  nvs_handle_t n;esp_err_t e=nvs_open("freslest",NVS_READWRITE,&n);
  if(e!=ESP_OK)return false;
  e=nvs_set_u8(n,"wifi",wifi);if(e==ESP_OK)e=nvs_set_u8(n,"bt",bluetooth);
  if(e==ESP_OK)e=nvs_commit(n);
  nvs_close(n);
  if(e==ESP_OK){wifiEnabled=wifi;btEnabled=bluetooth;return true;}
  return false;
}
bool errorScreen(const char *message) {
  freslestSafetyError(message);int selected=0;releaseButtons();
  for(;;) {
    title("Recovery Error","UP/DOWN Select   OK Open");
    tft.setTextColor(TFT_RED,TH().bg);tft.setCursor(18,56);tft.printf("%.72s",message);
    row(0,selected,"Retry");row(1,selected,"Recovery");row(2,selected,"See Log");gfxPresent();
    for(;;){
      poll(millis());
      if(bUp.pressed()){selected=(selected+2)%3;break;}
      if(bDown.pressed()){selected=(selected+1)%3;break;}
      if(bBack.pressed()){releaseButtons();return false;}
      if(bOk.pressed()){
        if(selected<2){releaseButtons();return selected==0;}
        releaseButtons();viewLog();break;
      }
      delay(10);
    }
  }
}
bool confirmDowndate(const char *version) {
  title("Freslest Downdate","OK Confirm   BACK Cancel");
  tft.setTextColor(TH().text,TH().bg);tft.setCursor(24,80);tft.printf("Return to Freslest OS %.31s?",version);
  tft.setCursor(24,112);tft.print("This version may contain bugs.");
  tft.setCursor(24,136);tft.print("Files, Wi-Fi settings and game saves are retained.");gfxPresent();
  releaseButtons();
  for(;;){poll(millis());if(bBack.pressed()){releaseButtons();return false;}
    if(bOk.pressed()){releaseButtons();return true;}delay(10);}
}
}

void freslestSafetyBegin() {
  if(logRing.magic!=MAGIC || logRing.head>=10 || logRing.count>10) {
    memset(&logRing,0,sizeof(logRing));logRing.magic=MAGIC;
  }
  for(auto &line:logRing.line)line[95]=0;
  originalLog=esp_log_set_vprintf(logOutput);
  storeOk=nvs_flash_init()==ESP_OK;
  nvs_handle_t n;size_t size=sizeof(record);
  if(!storeOk || nvs_open("fl_safe",NVS_READONLY,&n)!=ESP_OK)record={};
  else {
    esp_err_t e=nvs_get_blob(n,"boot",&record,&size);nvs_close(n);
    if(e!=ESP_OK || size!=sizeof(record) || record.magic!=MAGIC || record.stage>9 || record.policy.failures>3 || record.policy.armed>1 || record.policy.recovery>1)record={};
  }
  record.magic=MAGIC;record.lastStage=record.stage;
  record.error[sizeof(record.error)-1]=0;
  uint8_t clean=0;
  if(storeOk && nvs_open("fl_safe",NVS_READWRITE,&n)==ESP_OK){
    nvs_get_u8(n,"clean",&clean);
    if(nvs_set_u8(n,"clean",0)!=ESP_OK || nvs_commit(n)!=ESP_OK)storeOk=false;
    nvs_close(n);
  }
  if(clean==1)record.policy.cleanExit();
  auto r=esp_reset_reason();record.lastReset=(uint8_t)r;
  if(r==ESP_RST_DEEPSLEEP){
    for(int pin:{BTN_OK,BTN_BACK}){
      rtc_gpio_hold_dis((gpio_num_t)pin);rtc_gpio_deinit((gpio_num_t)pin);pinMode(pin,INPUT_PULLUP);
    }
    // A BACK wake must not be mistaken for the BACK+RESET LCD override.
    while(digitalRead(BTN_OK)==LOW || digitalRead(BTN_BACK)==LOW)delay(10);
  }
  bool crash=r==ESP_RST_PANIC||r==ESP_RST_INT_WDT||r==ESP_RST_TASK_WDT||r==ESP_RST_WDT;
  record.policy.begin(r==ESP_RST_DEEPSLEEP,crash);
  pinMode(BTN_LEFT,INPUT_PULLUP);pinMode(BTN_RIGHT,INPUT_PULLUP);
  if(digitalRead(BTN_LEFT)==LOW && digitalRead(BTN_RIGHT)==LOW)record.policy.recovery=1;
  if(!storeOk)record.policy.recovery=1;
  record.stage=(uint8_t)FreslestBootStage::Boot;
  if(!save()){record.policy.recovery=1;logLine("Boot journal write failed");}
  char text[96];snprintf(text,sizeof(text),"Reset: %s; previous: %s; failures: %u",resetName(r),stageName(record.lastStage),record.policy.failures);logLine(text);Serial.println(text);
  esp_register_shutdown_handler(cleanExit);
  idle.begin(millis());
}
void freslestSafetyStage(FreslestBootStage stage) {
  record.stage=(uint8_t)stage;
  if(!healthyBoot)save();
  logLine(stageName(record.stage));
}
void freslestSafetyError(const char *message) {
  strlcpy(record.error,message,sizeof(record.error));logLine(message);save();Serial.println(message);
}
bool freslestRecoveryRequested(){return record.policy.recovery;}
void freslestRecoveryRun(const char *error) {
  inRecovery=true;record.policy.recovery=1;save();releaseButtons();
  if(error && errorScreen(error)){
    record.policy.resume();save();ESP.restart();
  }
  char version[32]{};auto previous=previousOs(version,sizeof(version));int selected=0;
  for(;;) {
    title("Freslest Recovery","UP/DOWN Select   OK Change/Open   BACK stays here");
    tft.setTextSize(1);tft.setTextColor(TH().dim,TH().bg);tft.setCursor(18,54);
    tft.print("Wi-Fi and Bluetooth hardware stay OFF in Recovery.");
    tft.setCursor(18,70);tft.print("Radio switches below apply only after Continue.");
    row(0,selected,"Continue Freslest");
    char text[90];snprintf(text,sizeof(text),previous?"Freslest Downdate: %s":"Freslest Downdate: unavailable",version);row(1,selected,text,previous!=nullptr);
    row(2,selected,"See Log");
    snprintf(text,sizeof(text),"Wi-Fi after exit: %s",wifiEnabled?"ON":"OFF");row(3,selected,text);
    snprintf(text,sizeof(text),"Bluetooth after exit: %s",btEnabled?"ON":"OFF");row(4,selected,text);gfxPresent();
    for(;;) {
      poll(millis());
      if(bUp.pressed()){selected=(selected+4)%5;break;}
      if(bDown.pressed()){selected=(selected+1)%5;break;}
      if(bOk.pressed() || ((bLeft.pressed()||bRight.pressed()) && selected>=3)) {
        releaseButtons();
        if(selected==0) {
          record.policy.resume();record.stage=(uint8_t)FreslestBootStage::Settings;
          if(!save()){record.policy.recovery=1;errorScreen("Cannot save recovery settings. NVS is unavailable.");break;}
          inRecovery=false;stable=false;idle.begin(millis());return;
        }
        if(selected==1 && previous && confirmDowndate(version)) {
          bool retry;
          do {
            esp_err_t e=esp_ota_set_boot_partition(previous);
            if(e==ESP_OK){record.policy.recovery=0;cleanExit();ESP.restart();}
            char msg[96];snprintf(msg,sizeof(msg),"Downdate rejected: %s",esp_err_to_name(e));retry=errorScreen(msg);
          } while(retry);
        } else if(selected==2)viewLog();
        else if(selected>=3){
          bool retry;
          do {
            if(setRadios(selected==3?!wifiEnabled:wifiEnabled,selected==4?!btEnabled:btEnabled))break;
            retry=errorScreen("Cannot save Wi-Fi / Bluetooth settings.");
          }while(retry);
        }
        break;
      }
      delay(10);
    }
  }
}
void freslestSafetyTick(uint32_t now,bool healthy) {
  if(healthyBoot || inRecovery)return;
  if(!healthy){stable=false;return;}
  if(!stable){stable=true;stableAt=now;}
  if((uint32_t)(now-stableAt)<20000u)return;
  record.policy.healthy();record.stage=(uint8_t)FreslestBootStage::Ready;
  if(!save()){record.policy.armed=1;stableAt=now;return;}
  healthyBoot=true;logLine("Startup stable for 20 seconds");
}
void freslestIdleUpdate(uint32_t now) {
  bool activity=false;
  for(Button *b : {&bUp,&bDown,&bLeft,&bRight,&bOk,&bBack,&bFire})activity|=b->down()||b->pressed();
  activity|=(uint32_t)(now-usbInputLastActivity())<100u;
  auto disk=usbStorageInfo();
  bool blocked=screenMirrorActive()||inRecovery||!healthyBoot||usbInputDeviceMode()||app==APP_USB_TRANSFER||app==APP_FRESLEST_UPDATE||app==APP_PAINT||app==APP_NOTE||app==APP_CAMERA||wifiPortalActive()||nativeBrowserStatus().busy||disk.present();
  if(!idle.tick(now,activity,blocked))return;
  // OK and BACK are RTC GPIOs on ESP32-S3. Restore their digital mode on wake.
  constexpr uint64_t wake=(1ULL<<BTN_OK)|(1ULL<<BTN_BACK);
  if(digitalRead(BTN_OK)==LOW||digitalRead(BTN_BACK)==LOW){idle.begin(now);return;}
  for(int pin : {BTN_OK,BTN_BACK}){
    rtc_gpio_init((gpio_num_t)pin);rtc_gpio_set_direction((gpio_num_t)pin,RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en((gpio_num_t)pin);rtc_gpio_pulldown_dis((gpio_num_t)pin);
  }
  esp_err_t e=esp_sleep_enable_ext1_wakeup_io(wake,ESP_EXT1_WAKEUP_ANY_LOW);
  if(e!=ESP_OK){
    for(int pin:{BTN_OK,BTN_BACK}){rtc_gpio_deinit((gpio_num_t)pin);pinMode(pin,INPUT_PULLUP);}
    freslestSafetyError("Deep-sleep wake setup failed; staying awake.");idle.begin(now);return;
  }
  if(!usbStoragePrepareSleep()){
    for(int pin:{BTN_OK,BTN_BACK}){rtc_gpio_deinit((gpio_num_t)pin);pinMode(pin,INPUT_PULLUP);}
    idle.begin(now);return;
  }
  if(freslestBleInfo().enabled){
    freslestBleStop();uint32_t start=millis();
    while(strcmp(freslestBleInfo().status,"Bluetooth is off.") && (uint32_t)(millis()-start)<1500u)delay(10);
  }
  if(esp_bluedroid_get_status()==ESP_BLUEDROID_STATUS_ENABLED && esp_bluedroid_disable()!=ESP_OK){
    freslestSafetyError("Bluetooth could not stop. Recovery requested.");record.policy.recovery=1;save();ESP.restart();return;
  }
  if(esp_bt_controller_get_status()==ESP_BT_CONTROLLER_STATUS_ENABLED && esp_bt_controller_disable()!=ESP_OK){
    freslestSafetyError("Bluetooth controller could not stop. Recovery requested.");record.policy.recovery=1;save();ESP.restart();return;
  }
  if(!WiFi.mode(WIFI_OFF)){
    freslestSafetyError("Wi-Fi could not stop. Recovery requested.");record.policy.recovery=1;save();ESP.restart();return;
  }
  settingsSave();freslestSafetyStage(FreslestBootStage::Sleep);record.policy.cleanExit();save();cleanExit();
  title("Deep Sleep","Press the physical OK or BACK button to wake.");gfxPresent();
  audioStop();for(int i=0;i<8;++i){audioUpdate(millis());delay(30);}
  LittleFS.end();tft.endWrite();backlightSet(0);Serial.flush();
  // Deep sleep resets the CPU/peripherals and turns off radio/USB/AV clocks.
  // No external drive is mounted here, and updater/MSC/unsaved editors block it.
  esp_deep_sleep_start();
}
