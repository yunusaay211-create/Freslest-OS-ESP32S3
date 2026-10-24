// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_log.h"
#include "board_config.h"
#include "spi_lcd.h"
#include "freslest_ota.h"
#include "freslest_hid.h"
extern int doom_main(int argc, const char *const *argv);
extern void doom_wad_init(void);
extern void freslest_audio_mute_early(void);
static void doom_task(void *arg)
{
#if DOOM_START_IN_LEVEL
    const char *argv[]={"doom", "-warp", "1", "1", "-skill", "2", NULL};
#else
    const char *argv[]={"doom", NULL};
#endif
    doom_main(sizeof(argv)/sizeof(argv[0])-1, argv);
    vTaskDelete(NULL);
}
void app_main(void)
{
    if(!freslest_ota_boot_begin())return;
    /* External 100k pulldown on the amp EN pin gives reset-time silence. */
    freslest_audio_mute_early();
    ESP_LOGI("DOOM", "Freslest Doom S3 / IDF 5.4.x");
    if (!esp_psram_is_initialized() || esp_psram_get_size() < 4*1024*1024) {
        ESP_LOGE("DOOM", "PSRAM missing. N16R8 requires Octal PSRAM settings.");
        return;
    }
    ESP_LOGI("DOOM", "PSRAM size=%u free=%u", (unsigned)esp_psram_get_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    spi_lcd_init();
    doom_wad_init();
    if(!freslest_hid_start())ESP_LOGW("DOOM","%s",freslest_hid_status());
    if (xTaskCreatePinnedToCore(doom_task, "doom", 32768, NULL, 5, NULL, 1) != pdPASS)
        ESP_LOGE("DOOM", "Cannot allocate Doom task stack");
}
