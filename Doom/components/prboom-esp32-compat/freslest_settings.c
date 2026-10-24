// Freslest menu preferences stored separately from Doom savegames.
// Espressif NVS includes CRC and wear leveling.
#include <stdint.h>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freslest_settings.h"

int freslest_fullscreen = 1; // New 4:3 scale profile defaults ON (separate NVS key).
static int s_nvs_ready = 0;

static int prefs_ready(void)
{
    if (s_nvs_ready) return 1;
    esp_err_t err = nvs_flash_init();
    // Do not erase NVS on unexpected failures; it may hold other settings.
    if (err != ESP_OK) return 0;
    s_nvs_ready = 1;
    return 1;
}

void freslest_settings_load(int *sfx, int *music, int *messages, int *view)
{
    if (!prefs_ready()) return;
    nvs_handle_t h;
    if (nvs_open("freslest", NVS_READONLY, &h) != ESP_OK) return;
    uint8_t v;
    if (nvs_get_u8(h, "sfx", &v) == ESP_OK && v <= 15) *sfx = v;
    if (nvs_get_u8(h, "music", &v) == ESP_OK && v <= 15) *music = v;
    if (nvs_get_u8(h, "msg", &v) == ESP_OK && v <= 1) *messages = v;
    if (nvs_get_u8(h, "view", &v) == ESP_OK && v >= 3 && v <= 11) *view = v;
    if (nvs_get_u8(h, "scale43", &v) == ESP_OK && v <= 1) freslest_fullscreen = v;
    nvs_close(h);
}

void freslest_settings_save(int sfx, int music, int messages, int view)
{
    if (!prefs_ready()) return;
    nvs_handle_t h;
    if (nvs_open("freslest", NVS_READWRITE, &h) != ESP_OK) return;
    if (sfx < 0) sfx = 0;
    if (sfx > 15) sfx = 15;
    if (music < 0) music = 0;
    if (music > 15) music = 15;
    if (view < 3) view = 3;
    if (view > 11) view = 11;
    nvs_set_u8(h, "sfx", (uint8_t)sfx);
    nvs_set_u8(h, "music", (uint8_t)music);
    nvs_set_u8(h, "msg", (uint8_t)(messages != 0));
    nvs_set_u8(h, "view", (uint8_t)view);
    nvs_set_u8(h, "scale43", (uint8_t)(freslest_fullscreen != 0));
    nvs_commit(h);
    nvs_close(h);
}
