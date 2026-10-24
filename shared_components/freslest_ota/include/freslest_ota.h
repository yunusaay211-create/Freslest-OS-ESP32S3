#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef FRESLEST_OTA_HOST_TEST
#include "ota_test_platform.h"
#else
#include "esp_err.h"
#include "esp_partition.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define FRESLEST_OS_SLOT_SIZE 0x1b0000u
bool freslest_ota_boot_begin(void);
bool freslest_ota_health_tick(bool healthy);
bool freslest_ota_ready(void);
bool freslest_ota_layout_valid(void);
bool freslest_ota_take_update_request(void);
esp_err_t freslest_ota_request_update(void);
const esp_partition_t *freslest_ota_inactive_os(void);
const esp_partition_t *freslest_ota_active_os(void);
const esp_partition_t *freslest_ota_previous_os(void);
esp_err_t freslest_ota_begin_update(const esp_partition_t *target);
esp_err_t freslest_ota_select_update(const esp_partition_t *target);
void freslest_ota_cancel_update(void);
esp_err_t freslest_ota_restore_previous(void);
bool freslest_ota_recovered(void);
#ifdef __cplusplus
}
#endif
