// Return from Doom Quit to the verified factory OS; preserve NVS, WAD and saves.
#include "esp_ota_ops.h"
#include "freslest_ota.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_log.h"
extern void I_ShutdownSound(void);
void freslest_return_to_os(void) {
  if(!freslest_ota_ready())return;
  const esp_partition_t* os=freslest_ota_active_os();
  if(!os) {ESP_LOGE("OS","Confirmed OS partition missing");return;}
  esp_err_t err=esp_ota_set_boot_partition(os);
  if(err!=ESP_OK) {ESP_LOGE("OS","Cannot boot OS: %s",esp_err_to_name(err));return;}
  I_ShutdownSound();
  esp_restart();
}
