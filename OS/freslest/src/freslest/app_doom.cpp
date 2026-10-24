#include "app_doom.h"
#include "apps.h"
#include "audio_manager.h"
#include "../../gfx.h"
#include "../../input.h"
#include "settings.h"
#include <esp_ota_ops.h>
#include <freslest_ota.h>
#include <esp_partition.h>
#include <LittleFS.h>
#include "../../usb_input.h"
#include <cstring>
static bool doomRequested=false;
static void doomStatus(const char *text,bool error=false){
  tft.fillRect(20,184,440,20,TH().bg);tft.setTextSize(1);
  tft.setTextColor(error?TFT_RED:TH().text,TH().bg);tft.setCursor(24,190);tft.print(text);
}
void doomEnter() {
  doomRequested=false;
  tft.fillScreen(TH().bg);drawHeader("Doom");clearBody();drawFooter("OK Start", "BACK Game");
  tft.setTextSize(2);tft.setTextColor(TH().text,TH().bg);tft.setCursor(28,80);tft.print("DOOM");
  tft.setTextSize(1);tft.setCursor(28,120);tft.print("D-pad: move | FIRE: shoot | OK: use");
  tft.setCursor(28,144);tft.print("Doom > Quit Game > OK: return to Freslest OS.");
  tft.setCursor(28,168);tft.print("USB: WASD move | mouse turn/fire | E use");
  tft.fillRoundRect(100,210,280,60,10,TH().selbg);tft.setCursor(150,232);tft.setTextSize(2);tft.print("OK: OPEN DOOM");
}
void doomLoop(unsigned long now) {
  (void)now;
  bool open=bOk.pressed()||usbInputHit(100,210,280,60);
  if(!open&&!doomRequested)return;
  if(!freslest_ota_ready()){
    if(!doomRequested)doomStatus("Checking startup... Doom will open when ready.");
    doomRequested=true;return;
  }
  doomRequested=false;
  const esp_partition_t* p=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_2,"doom");
  esp_app_desc_t desc={};
  const char *stage="read-image";
  esp_err_t err=p?esp_ota_get_partition_description(p,&desc):ESP_ERR_NOT_FOUND;
  if(err==ESP_OK&&strncmp(desc.project_name,"freslest_doom_s3",sizeof(desc.project_name)))err=ESP_ERR_OTA_VALIDATE_FAILED;
  if(err==ESP_OK){stage="boot-check";err=esp_ota_set_boot_partition(p);}
  if(err!=ESP_OK) {
    doomStatus(!p?"Doom partition missing. Run flash_all.bat from full package.":
      "Doom image invalid. Run repair_doom.bat COMx from full package.",true);
    Serial.printf("DOOM launch failed: stage=%s error=%s; expected freslest_doom_s3 at 0x20000.\n",stage,esp_err_to_name(err));return;
  }
  doomStatus("Starting Doom...");gfxPresent();
  audioStop();for(int i=0;i<14;i++) {audioUpdate(millis());delay(110);}
  LittleFS.end();tft.endWrite();gfxPresent();ESP.restart();
}
