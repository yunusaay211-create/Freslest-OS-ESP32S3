// SPDX-License-Identifier: GPL-2.0-or-later
// Freslest Doom S3: PrBoom .dsg save files in a dedicated raw flash partition.
// Layout: 8 slots * 256 KiB; first 4 KiB of each slot is an integrity header.
// Writes payload first and commits the header last (no torn save appears valid).
// Replacing an existing slot is NOT power-loss atomic: keep another slot as backup.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freslest_save.h"

#define SAVE_MAGIC 0x46535631u  /* FSV1 */
#define SAVE_VERSION 1u
#define SAVE_CHUNK 4096u
#define SAVE_DESCRIPTION_SIZE 24u

static const char *TAG = "FRESLEST_SAVE";

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t slot;
    uint32_t length;
    uint32_t crc32;
    uint32_t reserved[3];
} save_header_t;

static const esp_partition_t *save_partition(void)
{
    const esp_partition_t *p = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x40, "saves");
    if (!p || p->size < (FRESLEST_SAVE_SLOTS * FRESLEST_SAVE_SLOT_SIZE)) {
        ESP_LOGE(TAG, "Missing/undersized saves partition. Flash new partitions.csv");
        return NULL;
    }
    return p;
}

int freslest_save_slot_from_path(const char *path)
{
    if (!path) return -1;
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *backslash = strrchr(name, '\\');
    if (backslash) name = backslash + 1;
    // Accept only names of the form prbmsavN.dsg or demosavN.dsg.
    const char *prefix = NULL;
    if (!strncmp(name, "prbmsav", 7)) prefix = name + 7;
    else if (!strncmp(name, "demosav", 7)) prefix = name + 7;
    if (!prefix || prefix[0] < '0' || prefix[0] >= '0' + FRESLEST_SAVE_SLOTS ||
        strcmp(prefix + 1, ".dsg") != 0) return -1;
    return prefix[0] - '0';
}

static uint32_t update_crc32(uint32_t crc, const uint8_t *data, size_t len)
{
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

static int get_header(const esp_partition_t *part, int slot, save_header_t *hdr)
{
    if (!part || slot < 0 || slot >= FRESLEST_SAVE_SLOTS) return 0;
    size_t offset = (size_t)slot * FRESLEST_SAVE_SLOT_SIZE;
    if (esp_partition_read(part, offset, hdr, sizeof(*hdr)) != ESP_OK) return 0;
    return hdr->magic == SAVE_MAGIC && hdr->version == SAVE_VERSION &&
           hdr->slot == (uint32_t)slot && hdr->length >= SAVE_DESCRIPTION_SIZE &&
           hdr->length <= FRESLEST_SAVE_MAX_SIZE;
}

int freslest_save_description(int slot, char *out, size_t cap)
{
    if (!out || cap < SAVE_DESCRIPTION_SIZE) return 0;
    const esp_partition_t *p = save_partition();
    save_header_t hdr;
    if (!get_header(p, slot, &hdr)) return 0;
    size_t off = (size_t)slot * FRESLEST_SAVE_SLOT_SIZE + FRESLEST_SAVE_HEADER_SIZE;
    if (esp_partition_read(p, off, out, SAVE_DESCRIPTION_SIZE) != ESP_OK) return 0;
    out[SAVE_DESCRIPTION_SIZE - 1] = 0;
    return 1;
}

int freslest_save_size(const char *path)
{
    const int slot = freslest_save_slot_from_path(path);
    const esp_partition_t *p = save_partition();
    save_header_t hdr;
    if (!get_header(p, slot, &hdr)) return -1;
    return (int)hdr.length;
}

int freslest_save_read(const char *path, void *dst, size_t len)
{
    const int slot = freslest_save_slot_from_path(path);
    const esp_partition_t *p = save_partition();
    save_header_t hdr;
    if (!dst || !get_header(p, slot, &hdr) || len != hdr.length) return 0;

    // Stage reads in internal RAM in case the Doom zone lives in external PSRAM.
    uint8_t *stage = heap_caps_malloc(SAVE_CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!stage) return 0;
    size_t off = (size_t)slot * FRESLEST_SAVE_SLOT_SIZE + FRESLEST_SAVE_HEADER_SIZE;
    uint32_t crc = 0xFFFFFFFFu;
    uint8_t *bytes = (uint8_t *)dst;
    for (size_t pos = 0; pos < len; pos += SAVE_CHUNK) {
        size_t chunk = len - pos;
        if (chunk > SAVE_CHUNK) chunk = SAVE_CHUNK;
        if (esp_partition_read(p, off + pos, stage, chunk) != ESP_OK) {
            heap_caps_free(stage);
            return 0;
        }
        memcpy(bytes + pos, stage, chunk);
        crc = update_crc32(crc, stage, chunk);
    }
    heap_caps_free(stage);
    crc ^= 0xFFFFFFFFu;
    if (crc != hdr.crc32) {
        ESP_LOGE(TAG, "Slot %d CRC mismatch. Save is invalid", slot + 1);
        return 0;
    }
    ESP_LOGI(TAG, "Loaded slot %d (%u bytes)", slot + 1, (unsigned)len);
    return 1;
}

int freslest_save_write(const char *path, const void *src, size_t len)
{
    int slot = freslest_save_slot_from_path(path);
    const esp_partition_t *p = save_partition();
    if (!p || slot < 0 || !src || len < SAVE_DESCRIPTION_SIZE ||
        len > FRESLEST_SAVE_MAX_SIZE) {
        ESP_LOGE(TAG, "Save failed: slot=%d size=%u max=%u", slot,
                 (unsigned)len, (unsigned)FRESLEST_SAVE_MAX_SIZE);
        return 0;
    }
    // Staging buffer in internal RAM: flash writes can temporarily disable PSRAM.
    uint8_t *stage = heap_caps_malloc(SAVE_CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!stage) {
        ESP_LOGE(TAG, "Not enough internal RAM for flash staging buffer");
        return 0;
    }

    const size_t base = (size_t)slot * FRESLEST_SAVE_SLOT_SIZE;
    esp_err_t err = esp_partition_erase_range(p, base, FRESLEST_SAVE_SLOT_SIZE);
    const uint8_t *bytes = (const uint8_t *)src;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t pos = 0; err == ESP_OK && pos < len; pos += SAVE_CHUNK) {
        size_t chunk = len - pos;
        if (chunk > SAVE_CHUNK) chunk = SAVE_CHUNK;
        memcpy(stage, bytes + pos, chunk);
        crc = update_crc32(crc, stage, chunk);
        err = esp_partition_write(p, base + FRESLEST_SAVE_HEADER_SIZE + pos,
                                  stage, chunk);
    }
    crc ^= 0xFFFFFFFFu;
    if (err == ESP_OK) {
        save_header_t header = {
            .magic = SAVE_MAGIC, .version = SAVE_VERSION,
            .slot = (uint32_t)slot, .length = (uint32_t)len, .crc32 = crc
        };
        // Header written last. Interrupted writes have no valid magic.
        err = esp_partition_write(p, base, &header, sizeof(header));
    }
    heap_caps_free(stage);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Save write failed: %s", esp_err_to_name(err));
        return 0;
    }
    ESP_LOGI(TAG, "Saved slot %d (%u bytes)", slot + 1, (unsigned)len);
    return 1;
}
