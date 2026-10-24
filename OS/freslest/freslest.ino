/* Freslest OS + Doom - physical buttons, ESP32-S3 N16R8, ESP-IDF 5.4.4 / Arduino 3.2.1. */
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_app_desc.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
extern "C" char _ext_ram_bss_start[],_ext_ram_bss_end[],_bt_bss_start[],_bt_bss_end[];
#endif
#include <freslest_ota.h>
#include <freslest_video.h>
#include <freslest_av.h>
#include "src/freslest/storage_init.h"
#include "src/freslest/usb_storage.h"
#include "gfx.h"
#include "screen_mirror.h"
#include "input.h"
#include "settings.h"
#include "src/freslest/apps.h"
#include "src/freslest/audio_manager.h"
#include "src/freslest/system_status.h"
#include "src/freslest/freslest_update_launcher.h"
#include "boot_screen.h"
#include "usb_input.h"
#include "src/freslest/ble_advertising.h"
#include "src/freslest/wifi_manager.h"
#include "src/freslest/power_recovery.h"
static bool storageReady=false;
static uint32_t menuReadyAt=0;
static bool radioStartup=false,bluetoothStartup=false;
static int avLabReadKey(){return Serial.available()?Serial.read():-1;}
void setup() {
  Serial.begin(115200);
  Serial.printf("Freslest OS %s / ESP32-S3 N16R8\n",esp_app_get_description()->version);
  Serial.println("USB role: Settings > Display > USB PC Viewer. UART: programming/viewer/logs.");
#if CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
  Serial.printf("RADIO BSS: external=%u bytes; BT=%u bytes at %p (%s)\n",
    (unsigned)(_ext_ram_bss_end-_ext_ram_bss_start),(unsigned)(_bt_bss_end-_bt_bss_start),
    _bt_bss_start,esp_ptr_external_ram(_bt_bss_start)?"PSRAM":"INTERNAL");
#endif
  Serial.printf("RAM: INTERNAL free=%u; PSRAM free=%u\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  freslestSafetyBegin();
  bool recovery=freslestRecoveryRequested();
  if(!recovery && freslest_av_lab_requested())freslest_av_lab_run(avLabReadKey);
  freslestSafetyStage(FreslestBootStage::Display);
  inputBegin();gfxBegin();
  freslestSafetyStage(FreslestBootStage::Settings);
  settingsLoad();backlightSet((uint8_t)brightnessUI);
  if(recovery)freslestRecoveryRun();
  // Recovery runs before the OTA trial timer and before storage/USB/radios.
  if(!freslest_ota_boot_begin()){
    freslestRecoveryRun("OTA state unavailable. See Log before retrying.");
    ESP.restart();return;
  }
  freslestSafetyStage(FreslestBootStage::Storage);
  storageReady=freslestStorageEnsure();
  if(freslestUpdateRunRequested())return;
  freslestSafetyStage(FreslestBootStage::Usb);
  usbInputBegin();
  systemStatusBegin();audioInit();audioBootMelody();
  randomSeed(esp_random());fpsInit();drawMenuFull();
  if(usbInputDeviceMode())enterApp(APP_USB_TRANSFER);
  else if(usbInputResumeNes())enterApp(APP_NES_LIBRARY);
  gfxPresent();
  freslestSafetyStage(FreslestBootStage::Menu);menuReadyAt=millis();
}
void loop() {
  uint32_t now=millis();
  screenMirrorPoll();unsigned diag=screenMirrorDiagnostics();
  if(diag&1)audioTest();
  if(diag&2){audioPrintDiagnostics();freslest_video_diagnostics();freslestBleDiagnostics();wifiManagerDiagnostics();}
  audioUpdate(now);systemStatusUpdate(now);
  if(!radioStartup && (uint32_t)(now-menuReadyAt)>=2500u){
    radioStartup=true;wifiManagerAllowStartup();
  }
  // Avoid initializing two radio stacks together while DMA/USB are starting.
  if(!bluetoothStartup && (uint32_t)(now-menuReadyAt)>=6500u){
    bluetoothStartup=true;
    if(btEnabled){freslestSafetyStage(FreslestBootStage::Bluetooth);freslestBleStart();}
  }
  if(!frameReady(now)) {delay(1);return;}
  inputUpdate(now);
  bool gameActive=app==APP_NES_PLAYER||app==APP_DINO||app==APP_TETRIS||app==APP_SNAKE||app==APP_MINES||app==APP_2048;
  if(!gameActive)handleGlobalClick();
  fpsTick(now);taskSwitcherHandle(now);
  // Sensor reads happen before display transactions.
  appLoop(now);
  bool essentialsReady=psramFound()&&storageReady&&tft.width()==480&&tft.height()==gfxUiHeight();
  bool startupReady=false;
  if(!essentialsReady||!freslest_video_starting())
    startupReady=freslest_ota_health_tick(essentialsReady&&gfxHealthy());
  appInstallNoticeUpdate(now,startupReady);
  freslestSafetyTick(now,startupReady && bluetoothStartup);
  gfxPresent();
  freslestIdleUpdate(now);
  // A slow LCD frame must still leave idle time for watchdog servicing.
  delay(1);
}
