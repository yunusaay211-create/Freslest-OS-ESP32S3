#include "freslest_update_launcher.h"
#include "../../gfx.h"
#include "../../input.h"
#include "apps.h"
#include "audio_manager.h"
#include "freslest_update.h"
#include "settings.h"
#include <LittleFS.h>
#include <cstdlib>
#include <cstring>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <freslest_ota.h>
#include <nvs.h>

bool freslestUpdateRunRequested() {
  if (!freslest_ota_take_update_request())
    return false;
  if (xTaskCreatePinnedToCore(freslest_update_task, "freslest_update", 16384,
                              nullptr, 5, nullptr, 1) == pdPASS) {
    vTaskDelete(nullptr);
    return true;
  }
  Serial.println("E303: Cannot allocate updater task. Retry from the menu.");
  return false;
}
void freslestUpdateEnter() {
  tft.fillScreen(TH().bg);
  drawHeader("Freslest Update");
  clearBody();
  drawFooter("OK Retry", "BACK Menu");
  tft.setTextSize(2);
  tft.setTextColor(TH().text, TH().bg);
  tft.setCursor(24, 76);
  tft.print("OPENING USB UPDATE");
  tft.setTextSize(1);
  tft.setCursor(24, 120);
  tft.print("Switching the native USB port to host mode.");
  tft.setCursor(24, 144);
  tft.print("Insert a USB drive containing a Freslest ZIP.");
  esp_err_t e = freslest_ota_request_update();
  if (e != ESP_OK) {
    tft.setTextColor(TFT_RED, TH().bg);
    tft.setCursor(24, 188);
    if (!freslest_ota_ready())
      tft.print("Startup check: wait 10 seconds, then press OK.");
    else
      tft.printf("Cannot start updater: %s", esp_err_to_name(e));
    return;
  }
  gfxPresent();
  settingsSave();
  audioStop();
  for (int i = 0; i < 8; i++) {
    audioUpdate(millis());
    delay(110);
  }
  LittleFS.end();
  tft.endWrite();
  ESP.restart();
}
void freslestUpdateLoop(uint32_t now) {
  (void)now;
  if (bBack.pressed()) {
    enterApp(APP_MENU);
    drawMenuFull();
    return;
  }
  if (bOk.pressed())
    freslestUpdateEnter();
}
