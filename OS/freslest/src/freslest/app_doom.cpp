#include "app_doom.h"
#include "apps.h"
#include "audio_manager.h"
#include "doom_save_export.h"
#include "../../gfx.h"
#include "../../input.h"
#include "settings.h"
#include "storage_init.h"
#include "usb_storage.h"
#include <esp_ota_ops.h>
#include <freslest_ota.h>
#include <esp_partition.h>
#include <LittleFS.h>
#include "../../usb_input.h"
#include <cstring>
static bool doomRequested=false;
static int doomChoice=0;
static constexpr const char *doomOptions[] = {
  "Start Doom", "Export saves to SD card", "Export saves to USB drive", "Export saves to PC"
};
static constexpr int doomX=30, doomY=101, doomWidth=420, doomStep=28, doomHeight=24;
static void doomStatus(const char *text,bool error=false){
  tft.fillRect(20,221,440,28,TH().bg);tft.setTextSize(1);
  tft.setTextColor(error?TFT_RED:TH().text,TH().bg);tft.setCursor(24,226);tft.print(text);
}
static void drawDoomMenu() {
  tft.fillScreen(TH().bg);drawHeader("Doom");clearBody();drawFooter("UP/DOWN Select | OK Open", "BACK Games");
  tft.setTextSize(1);tft.setTextColor(TH().text,TH().bg);
  tft.setCursor(28,54);tft.print("D-pad: move | FIRE: shoot | OK: use");
  tft.setCursor(28,68);tft.print("USB: WASD move | mouse turn/fire | E use");
  tft.setCursor(28,82);tft.print("Quit Game returns to Freslest OS.");
  for (int i=0; i<4; ++i) {
    const int y=doomY+i*doomStep;
    tft.fillRoundRect(doomX,y,doomWidth,doomHeight,5,i==doomChoice?TH().selbg:TH().panel);
    tft.setCursor(doomX+10,y+8);tft.print(doomOptions[i]);
  }
}
void doomEnter() {
  doomRequested=false;
  doomChoice=0;
  drawDoomMenu();
}
static void exportDoomSaves(int choice) {
  if (choice==3) {
    if (!freslest_ota_ready()) { doomStatus("Startup check in progress. Try again shortly."); return; }
    if (!usbInputSwitchDoomExport()) doomStatus("Cannot start PC export. Try again.",true);
    return;
  }
  const bool usb=choice==2;
  if (usb) usbStorageBegin();
  if (usb ? !usbStorageLock() : !freslestSdMounted()) {
    doomStatus(usb?"Connect a FAT USB drive and try again.":"Insert the SD card and restart Freslest.",true);
    return;
  }
  doomStatus("Exporting saves...");
  gfxPresent();
  char folder[48], status[76];
  const DoomSaveExport result=doomSaveExportFiles(usb?usbStorageRoot():freslestSdRoot(),folder);
  if (usb) usbStorageUnlock();
  if (result.copied) {
    const char *relative=strchr(folder+1,'/');
    snprintf(status,sizeof(status),"Exported %u, failed %u: %.32s",result.copied,result.failed,relative?relative+1:folder);
    doomStatus(status,result.failed!=0);
  } else doomStatus(result.failed?"Save export failed. Original saves kept.":"No Doom saves found.",result.failed!=0);
}
void doomLoop(unsigned long now) {
  (void)now;
  if (bUp.pressed() || bDown.pressed()) {
    doomChoice=(doomChoice+(bUp.pressed()?3:1))%4;
    doomRequested=false;
    drawDoomMenu();
  }
  bool choose=bOk.pressed();
  for (int i=0; i<4; ++i)
    if (usbInputHit(doomX,doomY+i*doomStep,doomWidth,doomHeight)) { doomChoice=i; choose=true; break; }
  if (choose && doomChoice) { doomRequested=false; drawDoomMenu(); exportDoomSaves(doomChoice); return; }
  if (!choose&&!doomRequested) return;
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
