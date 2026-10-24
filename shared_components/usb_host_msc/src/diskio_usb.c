/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * FatFS diskio driver for USB MSC (ESP-IDF < 6.0).
 *
 * ff_diskio_register_msc() hangs SCSI-backed init/status/read/write/ioctl
 * callbacks on a FatFS drive number:
 *
 *   fopen / VFS -> FatFS -> usb_disk_* -> scsi_cmd_read10/write10 -> BOT/USB
 *
 * On ESP-IDF 6.0+ this file is not compiled. FatFS uses diskio_bdl.c instead
 * and MSC supplies an esp_blockdev handle from msc_host_get_blockdev().
 */

#include "diskio_usb.h"
#include "diskio_impl.h"
#include "esp_log.h"
#include "ff.h"
#include "ffconf.h"
#include "msc_common.h"
#include "msc_scsi_bot.h"
#include "usb/usb_types_stack.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static usb_disk_t *s_disks[FF_VOLUMES] = {NULL};

static uint32_t s_base[FF_VOLUMES], s_count[FF_VOLUMES];
static bool s_readonly[FF_VOLUMES];
typedef struct {
  uint32_t first, count;
} gpt_part;
static gpt_part s_parts[FF_VOLUMES][16];
static unsigned s_part_count[FF_VOLUMES];
static const char *TAG = "diskio_usb";

static DSTATUS usb_disk_initialize(BYTE pdrv) { return RES_OK; }

static DSTATUS usb_disk_status(BYTE pdrv) { return RES_OK; }

static DRESULT usb_disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count) {
  assert(pdrv < FF_VOLUMES);
  assert(s_disks[pdrv]);

  usb_disk_t *disk = s_disks[pdrv];
  size_t sector_size = disk->block_size;
  msc_device_t *dev = __containerof(disk, msc_device_t, disk);

  if (!count || sector >= s_count[pdrv] || count > s_count[pdrv] - sector)
    return RES_PARERR;
  esp_err_t err =
      scsi_cmd_read10(dev, buff, sector + s_base[pdrv], count, sector_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "scsi_cmd_read10 failed (%d)", err);
    return RES_ERROR;
  }

  return RES_OK;
}

static DRESULT usb_disk_write(BYTE pdrv, const BYTE *buff, DWORD sector,
                              UINT count) {
  assert(pdrv < FF_VOLUMES);
  assert(s_disks[pdrv]);

  usb_disk_t *disk = s_disks[pdrv];
  size_t sector_size = disk->block_size;
  msc_device_t *dev = __containerof(disk, msc_device_t, disk);

  if (s_readonly[pdrv])
    return RES_WRPRT;
  if (!count || sector >= s_count[pdrv] || count > s_count[pdrv] - sector)
    return RES_PARERR;
  esp_err_t err =
      scsi_cmd_write10(dev, buff, sector + s_base[pdrv], count, sector_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "scsi_cmd_write10 failed (%d)", err);
    return RES_ERROR;
  }
  return RES_OK;
}

static DRESULT usb_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
  assert(pdrv < FF_VOLUMES);
  assert(s_disks[pdrv]);

  usb_disk_t *disk = s_disks[pdrv];

  switch (cmd) {
  case CTRL_SYNC:
    return RES_OK;
  case GET_SECTOR_COUNT:
    *((DWORD *)buff) = s_count[pdrv];
    return RES_OK;
  case GET_SECTOR_SIZE:
    // FatFS sector window is sized from this WORD; refuse sizes that would
    // truncate or exceed FF_MAX_SS while disk_read still transfers block_size.
    if (disk->block_size < FF_MIN_SS || disk->block_size > FF_MAX_SS ||
        disk->block_size > UINT16_MAX ||
        (disk->block_size & (disk->block_size - 1)) != 0) {
      ESP_LOGE(TAG, "Unsupported block_size %" PRIu32,
               (uint32_t)disk->block_size);
      return RES_ERROR;
    }
    *((WORD *)buff) = (WORD)disk->block_size;
    return RES_OK;
  case GET_BLOCK_SIZE:
    return RES_ERROR;
  }
  return RES_ERROR;
}

void ff_diskio_register_msc(BYTE pdrv, usb_disk_t *disk) {
  assert(pdrv < FF_VOLUMES);

  static const ff_diskio_impl_t usb_disk_impl = {.init = &usb_disk_initialize,
                                                 .status = &usb_disk_status,
                                                 .read = &usb_disk_read,
                                                 .write = &usb_disk_write,
                                                 .ioctl = &usb_disk_ioctl};
  s_disks[pdrv] = disk;
  s_base[pdrv] = 0;
  s_count[pdrv] = disk->block_count;
  s_readonly[pdrv] = false;
  s_part_count[pdrv] = 0;
  ff_diskio_register(pdrv, &usb_disk_impl);
}

BYTE ff_diskio_get_pdrv_disk(const usb_disk_t *disk) {
  for (int i = 0; i < FF_VOLUMES; i++) {
    if (disk == s_disks[i]) {
      return i;
    }
  }
  return 0xff;
}

// GPT mapping is read-only. Validate both CRCs and every physical/logical bound
// before exposing a volume to FatFS. No partition metadata is ever rewritten.
static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static uint64_t le64(const uint8_t *p) {
  return le32(p) | ((uint64_t)le32(p + 4) << 32);
}
static uint32_t crc_update(uint32_t c, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (unsigned bit = 0; bit < 8; bit++)
      c = (c >> 1) ^ (0xedb88320u & -(c & 1u));
  }
  return c;
}
static bool physical_read(BYTE pdrv, uint32_t lba, uint8_t *out) {
  usb_disk_t *disk = s_disks[pdrv];
  if (!disk || lba >= disk->block_count)
    return false;
  msc_device_t *dev = __containerof(disk, msc_device_t, disk);
  return scsi_cmd_read10(dev, out, lba, 1, disk->block_size) == ESP_OK;
}
unsigned ff_diskio_msc_gpt_partitions(BYTE pdrv) {
  if (pdrv >= FF_VOLUMES || !s_disks[pdrv])
    return 0;
  usb_disk_t *disk = s_disks[pdrv];
  unsigned ss = disk->block_size;
  if (ss < 512 || ss > 4096 || (ss & (ss - 1)))
    return 0;
  uint8_t *block = malloc(ss);
  if (!block)
    return 0;
  s_part_count[pdrv] = 0;
  bool valid = false;
  if (!physical_read(pdrv, 1, block) || memcmp(block, "EFI PART", 8) ||
      le32(block + 8) != 0x10000)
    goto done;
  uint32_t header = le32(block + 12), checksum = le32(block + 16);
  if (header < 92 || header > ss || le64(block + 24) != 1)
    goto done;
  memset(block + 16, 0, 4);
  if ((crc_update(0xffffffff, block, header) ^ 0xffffffff) != checksum)
    goto done;
  uint64_t first = le64(block + 40), last = le64(block + 48),
           table = le64(block + 72);
  uint32_t entries = le32(block + 80), stride = le32(block + 84),
           table_crc = le32(block + 88);
  uint64_t bytes = (uint64_t)entries * stride;
  if (!entries || entries > 256 || stride < 128 || stride > 1024 ||
      stride % 128 || table < 2 || first > last || last >= disk->block_count ||
      table + (bytes + ss - 1) / ss > first)
    goto done;
  uint32_t crc = 0xffffffff;
  for (uint64_t pos = 0; pos < bytes; pos += ss) {
    if (!physical_read(pdrv, table + pos / ss, block))
      goto done;
    size_t n = bytes - pos;
    if (n > ss)
      n = ss;
    crc = crc_update(crc, block, n);
  }
  if ((crc ^ 0xffffffff) != table_crc)
    goto done;
  for (unsigned pass = 0; pass < 2; pass++)
    for (uint32_t i = 0; i < entries && s_part_count[pdrv] < 16; i++) {
      uint64_t at = (uint64_t)i * stride;
      uint8_t entry[128];
      if (!physical_read(pdrv, table + at / ss, block))
        goto done;
      // stride is a multiple of 128 and all supported sectors are multiples of
      // 512.
      memcpy(entry, block + at % ss, sizeof(entry));
      static const uint8_t basic[16] = {0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9,
                                        0x33, 0x44, 0x87, 0xc0, 0x68, 0xb6,
                                        0xb7, 0x26, 0x99, 0xc7};
      bool is_basic = !memcmp(entry, basic, 16);
      if (is_basic != (pass == 0))
        continue;
      uint64_t start = le64(entry + 32), end = le64(entry + 40);
      bool used = false;
      for (unsigned k = 0; k < 16; k++)
        used |= entry[k] != 0;
      if (!used || start < first || end > last || end < start ||
          end > UINT32_MAX || !physical_read(pdrv, start, block))
        continue;
      unsigned bps = block[11] | (block[12] << 8), spc = block[13],
               fats = block[16], reserved = block[14] | (block[15] << 8);
      uint32_t total = block[19] | (block[20] << 8);
      if (!total)
        total = le32(block + 32);
      uint32_t fat = block[22] | (block[23] << 8);
      if (!fat)
        fat = le32(block + 36);
      if (block[510] != 0x55 || block[511] != 0xaa || bps != ss || !spc ||
          (spc & (spc - 1)) || spc > 128 || !reserved || fats < 1 || fats > 2 ||
          !fat || !total || total > end - start + 1 ||
          (uint64_t)reserved + (uint64_t)fats * fat >= total)
        continue;
      s_parts[pdrv][s_part_count[pdrv]++] = (gpt_part){start, end - start + 1};
    }
  valid = true;
done:
  free(block);
  if (!valid)
    s_part_count[pdrv] = 0;
  return s_part_count[pdrv];
}
bool ff_diskio_msc_select_partition(BYTE pdrv, unsigned index) {
  if (pdrv >= FF_VOLUMES || index >= s_part_count[pdrv])
    return false;
  s_base[pdrv] = s_parts[pdrv][index].first;
  s_count[pdrv] = s_parts[pdrv][index].count;
  s_readonly[pdrv] = true;
  return true;
}
void ff_diskio_unregister_msc(BYTE pdrv) {
  ff_diskio_unregister(pdrv);
  s_disks[pdrv] = NULL;
  s_base[pdrv] = s_count[pdrv] = s_part_count[pdrv] = 0;
  s_readonly[pdrv] = false;
}
