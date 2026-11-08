#include "storage_init.h"
#include "../../gfx.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <SD.h>
#include <esp_partition.h>
namespace {
const char *status = "Not mounted";
constexpr char sdRoot[] = "/sd";
bool sdMounted = false;
bool blank(const esp_partition_t *p) {
  if (!p || p->address != 0xdb0000 || p->size != 0x50000)
    return false;
  uint8_t block[1024];
  for (size_t offset = 0; offset < p->size; offset += sizeof(block)) {
    if (esp_partition_read(p, offset, block, sizeof(block)) != ESP_OK)
      return false;
    for (uint8_t byte : block)
      if (byte != 0xff)
        return false;
    delay(1);
  }
  return true;
}
} // namespace
bool freslestStorageEnsure() {
  if (LittleFS.totalBytes() ||
      LittleFS.begin(false, "/littlefs", 10, "spiffs")) {
    status = "Ready";
    return true;
  }
  const esp_partition_t *p = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "spiffs");
  if (!p)
    status = "E101: ROM partition missing";
  else if (!blank(p))
    status = "E102: ROM storage unreadable; files kept";
  else if (!LittleFS.begin(true, "/littlefs", 10, "spiffs"))
    status = "E103: Cannot initialize empty storage";
  else {
    status = "Ready";
    return true;
  }
  Serial.printf("Storage: %s\n", status);
  return false;
}
const char *freslestStorageStatus() { return status; }

void freslestSdBegin() {
  // Enter SD SPI mode with the TFT deselected before the first LCD transfer.
  // LovyanGFX later uses this same Arduino SPI bus and transaction mutex.
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  sdMounted = SPI.begin(PIN_TFT_SCK, PIN_SD_MISO, PIN_TFT_MOSI) &&
              SD.begin(PIN_SD_CS, SPI, 4000000, sdRoot, 4, false, true);
  if (!sdMounted)
    SPI.end();
  Serial.println(sdMounted ? "SD SPI: mounted /sd" : "SD SPI: USB fallback");
}

bool freslestSdMounted() { return sdMounted; }
const char *freslestSdRoot() { return sdRoot; }
