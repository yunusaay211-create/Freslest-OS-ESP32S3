#include "storage_init.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_partition.h>
namespace {
const char *status = "Not mounted";
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
