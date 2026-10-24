// SPDX-License-Identifier: MIT
// Runs from the confirmed OS and writes only the other OS partition.
#include "freslest_update.h"
#include "freslest_ota.h"
#include "freslest_update_package.h"
#include "update_display.h"
#include <ctype.h>
#include <dirent.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef FRESLEST_UPDATE_RUNTIME_TEST
#include "update_runtime_platform.h"
#else
#include "bootloader_common.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"
#include "usb/usb_host.h"
#endif

#define MAX_PACKAGES 12
#ifndef USB_ROOT
#define USB_ROOT "/usb"
#endif
static const char *TAG = "FreslestUpdate";
typedef struct {
  char filename[128];
  freslest_update_package package;
} candidate;
static candidate packages[MAX_PACKAGES];
static unsigned count, selected, skipped;
static uint8_t *screen;
static QueueHandle_t usb_events;
static msc_host_device_handle_t drive;
static msc_host_vfs_handle_t mounted;
static atomic_bool removed;
static bool confirm, restore_confirm, failed, host_ready;
static const esp_partition_t *previous;
static char previous_version[32];
static uint8_t last_usb_address;
static char error_text[100], last_rejection[100];
static int last_percent = -1;

// Compact 5x7 glyphs authored for this English-only recovery interface.
static const struct {
  char c;
  uint8_t bits[5];
} font[] = {
    {' ', {0, 0, 0, 0, 0}},
    {'!', {0, 0, 0x5f, 0, 0}},
    {'%', {0x63, 0x13, 8, 0x64, 0x63}},
    {'-', {8, 8, 8, 8, 8}},
    {'.', {0, 0x60, 0x60, 0, 0}},
    {'/', {0x20, 0x10, 8, 4, 2}},
    {':', {0, 0x36, 0x36, 0, 0}},
    {'?', {2, 1, 0x51, 9, 6}},
    {'>', {0, 0x41, 0x22, 0x14, 8}},
    {'(', {0, 0x1c, 0x22, 0x41, 0}},
    {')', {0, 0x41, 0x22, 0x1c, 0}},
    {'0', {0x3e, 0x51, 0x49, 0x45, 0x3e}},
    {'1', {0, 0x42, 0x7f, 0x40, 0}},
    {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
    {'3', {0x21, 0x41, 0x45, 0x4b, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7f, 0x10}},
    {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
    {'6', {0x3c, 0x4a, 0x49, 0x49, 0x30}},
    {'7', {1, 0x71, 9, 5, 3}},
    {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
    {'9', {6, 0x49, 0x49, 0x29, 0x1e}},
    {'A', {0x7e, 0x11, 0x11, 0x11, 0x7e}},
    {'B', {0x7f, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x3e, 0x41, 0x41, 0x41, 0x22}},
    {'D', {0x7f, 0x41, 0x41, 0x22, 0x1c}},
    {'E', {0x7f, 0x49, 0x49, 0x49, 0x41}},
    {'F', {0x7f, 9, 9, 9, 1}},
    {'G', {0x3e, 0x41, 0x49, 0x49, 0x7a}},
    {'H', {0x7f, 8, 8, 8, 0x7f}},
    {'I', {0, 0x41, 0x7f, 0x41, 0}},
    {'J', {0x20, 0x40, 0x41, 0x3f, 1}},
    {'K', {0x7f, 8, 0x14, 0x22, 0x41}},
    {'L', {0x7f, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7f, 2, 0x0c, 2, 0x7f}},
    {'N', {0x7f, 4, 8, 0x10, 0x7f}},
    {'O', {0x3e, 0x41, 0x41, 0x41, 0x3e}},
    {'P', {0x7f, 9, 9, 9, 6}},
    {'Q', {0x3e, 0x41, 0x51, 0x21, 0x5e}},
    {'R', {0x7f, 9, 0x19, 0x29, 0x46}},
    {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {1, 1, 0x7f, 1, 1}},
    {'U', {0x3f, 0x40, 0x40, 0x40, 0x3f}},
    {'V', {0x1f, 0x20, 0x40, 0x20, 0x1f}},
    {'W', {0x3f, 0x40, 0x38, 0x40, 0x3f}},
    {'X', {0x63, 0x14, 8, 0x14, 0x63}},
    {'Y', {7, 8, 0x70, 8, 7}},
    {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
    {'_', {0x40, 0x40, 0x40, 0x40, 0x40}},
    {'+', {8, 8, 0x3e, 8, 8}},
};
enum { BG = 0, PANEL = 1, TEXT = 2, DIM = 3, ACCENT = 4, SELECT = 5, RED = 6 };
static void rect(int x, int y, int w, int h, uint8_t c) {
  if (x < 0 || y < 0 || x + w > 480 || y + h > 320)
    return;
  for (int line = y; line < y + h; line++)
    memset(screen + line * 480 + x, c, w);
}
static void text(int x, int y, const char *s, int scale, uint8_t color) {
  int begin = x;
  for (; *s; s++) {
    if (*s == '\n') {
      x = begin;
      y += scale * 10;
      continue;
    }
    char c = (char)toupper((unsigned char)*s);
    const uint8_t *g = font[0].bits;
    for (unsigned i = 0; i < sizeof(font) / sizeof(font[0]); i++)
      if (font[i].c == c) {
        g = font[i].bits;
        break;
      }
    if (x + scale * 5 > 470 || y + scale * 7 > 320)
      break;
    for (int col = 0; col < 5; col++)
      for (int row = 0; row < 7; row++)
        if (g[col] & (1 << row))
          rect(x + col * scale, y + row * scale, scale, scale, color);
    x += scale * 6;
  }
}
static void base(const char *footer) {
  memset(screen, BG, 480 * 320);
  rect(0, 0, 480, 44, PANEL);
  rect(0, 296, 480, 24, PANEL);
  text(14, 7, "FRESLEST", 2, TEXT);
  text(14, 29, "Freslest Update", 1, DIM);
  text(14, 304, footer, 1, TEXT);
}
static void show(void) { freslest_update_draw_indexed(screen); }
static void draw(void) {
  base(restore_confirm ? "BACK Cancel restore        OK Restore"
       : confirm       ? "BACK Cancel update         OK Update"
       : failed        ? "OK Retry                   BACK Cancel"
                       : "UP/DOWN Select   OK Open    BACK Cancel");
  char line[100];
  if (restore_confirm) {
    text(24, 64, "RESTORE PREVIOUS OS?", 2, TEXT);
    snprintf(line, sizeof(line), "VERSION: %s", previous_version);
    text(24, 106, line, 2, ACCENT);
    text(24, 150, "Restart using the last confirmed OS version.", 1, TEXT);
    text(24, 178, "Your games, Note and saves are kept.", 1, DIM);
    text(24, 216, "BACK cancels. OK restores.", 2, TEXT);
  } else if (failed) {
    text(24, 68, "UPDATE STOPPED", 2, RED);
    text(24, 104, error_text, 1, TEXT);
    text(24, 138, "The running OS slot is still intact.", 1, DIM);
    text(24, 162, "Reinsert the USB drive, then press OK.", 1, DIM);
    text(24, 188, "The programming port can also reinstall OS.", 1, DIM);
  } else if (confirm && count) {
    text(24, 64, "ARE YOU SURE?", 3, TEXT);
    snprintf(line, sizeof(line), "VERSION: %s",
             packages[selected].package.version);
    text(24, 104, line, 2, ACCENT);
    text(24, 142, "This version may be buggy or damaged.", 1, TEXT);
    text(24, 164, "Only install a trusted Freslest ZIP.", 1, DIM);
    text(24, 196, "BACK cancels the update.", 2, DIM);
    text(24, 232, "OK starts updating Freslest OS.", 2, TEXT);
  } else if (!mounted) {
    text(24, 68, "INSERT USB DRIVE", 2, TEXT);
    text(24, 110, "Insert a USB flash drive containing a", 1, TEXT);
    text(24, 130, "compatible Freslest ZIP into USB / OTG.", 1, TEXT);
    text(24, 166, "FAT16 / FAT32. Keep the ZIP unextracted.", 1, DIM);
    text(24, 190, "Use an OTG adapter with 5V USB power.", 1, DIM);
    text(24, 214, "Use the other port to power the board.", 1, DIM);
    if (previous)text(24,232,"FIRE: restore the previous OS version",1,ACCENT);
    if (error_text[0])
      text(24, 250, error_text, 1, RED);
  } else if (!count) {
    text(24, 68, "NO COMPATIBLE ZIP", 2, TEXT);
    text(24, 110, "Copy a Freslest Update ZIP to the USB root.", 1, DIM);
    text(24, 134, "Reinsert the drive or press OK to rescan.", 1, DIM);
    if (last_rejection[0])
      text(24, 170, last_rejection, 1, RED);
    if (previous)
      text(24, 214, "FIRE: restore the previous OS version", 1, ACCENT);
  } else {
    snprintf(line, sizeof(line), "%u compatible update%s", count,
             count == 1 ? "" : "s");
    text(24, 56, line, 1, DIM);
    unsigned first = (selected / 4) * 4;
    for (unsigned row = 0; row < 4 && first + row < count; row++) {
      unsigned i = first + row;
      int y = 78 + row * 47;
      if (i == selected)
        rect(18, y - 5, 444, 43, SELECT);
      snprintf(line, sizeof(line), "%s%s", i == selected ? "> " : "  ",
               packages[i].package.version);
      text(28, y, line, 2, i == selected ? TEXT : DIM);
      char filename[66];
      snprintf(filename, sizeof(filename), "%.64s", packages[i].filename);
      text(40, y + 20, filename, 1, DIM);
    }
    snprintf(line, sizeof(line), "%u/%u  Skipped: %u%s", selected + 1, count,
             skipped, previous ? "  FIRE: previous OS" : "");
    text(24, 278, line, 1, DIM);
  }
  show();
}
static void progress(unsigned percent, const char *phase) {
  if (percent > 100)
    percent = 100;
  if ((int)percent == last_percent)
    return;
  last_percent = (int)percent;
  base("THIS MAY TAKE A LONG TIME");
  text(24, 76, "UPDATING FRESLEST OS", 2, TEXT);
  text(24, 116, phase, 1, DIM);
  rect(24, 152, 432, 32, DIM);
  rect(27, 155, 426, 26, BG);
  rect(27, 155, (426 * percent) / 100, 26, ACCENT);
  char label[24];
  snprintf(label, sizeof(label), "%u%%", percent);
  text(212, 202, label, 3, TEXT);
  text(24, 258, "Keep the device powered. Do not press RESET.", 1, DIM);
  show();
}
static const esp_partition_t *os_partition(void) {
  return freslest_ota_inactive_os();
}
static void unmount_drive(void) {
  if (mounted) {
    msc_host_vfs_unregister(mounted);
    mounted = NULL;
  }
  if (drive) {
    msc_host_uninstall_device(drive);
    drive = NULL;
  }
  count = selected = 0;
  confirm = false;
}
static void return_to_os(void) {
  const esp_partition_t *running = esp_ota_get_running_partition(),
                        *boot = esp_ota_get_boot_partition();
  esp_err_t e = running ? ESP_OK : ESP_ERR_NOT_FOUND;
  if (running && (!boot || boot->address != running->address))
    e = esp_ota_set_boot_partition(running);
  if (e != ESP_OK) {
    snprintf(error_text, sizeof(error_text), "Cannot select current OS: %s",
             esp_err_to_name(e));
    failed = true;
    confirm = restore_confirm = false;
    draw();
    return;
  }
  freslest_ota_cancel_update();
  unmount_drive();
  base("UPDATE CANCELLED");
  text(24, 112, "RETURNING TO FRESLEST OS", 2, TEXT);
  show();
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
}
static void msc_callback(const msc_host_event_t *event, void *context) {
  (void)context;
  if (event->event == MSC_DEVICE_DISCONNECTED)
    atomic_store(&removed, true);
  // Never unmount or block the USB class driver from its callback.
  if (usb_events && (event->event == MSC_DEVICE_CONNECTED ||
                     event->event == MSC_DEVICE_DISCONNECTED))
    xQueueSend(usb_events, event, 0);
}
static void usb_daemon(void *unused) {
  (void)unused;
  for (;;) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();
  }
}
static bool start_host(void) {
  usb_events = xQueueCreate(8, sizeof(msc_host_event_t));
  if (!usb_events)
    return false;
  usb_host_config_t cfg = {.intr_flags = ESP_INTR_FLAG_LEVEL1};
  esp_err_t e = usb_host_install(&cfg);
  if (e != ESP_OK) {
    snprintf(error_text, sizeof(error_text), "USB host: %s",
             esp_err_to_name(e));
    return false;
  }
  const msc_host_driver_config_t msc = {.create_backround_task = true,
                                        .task_priority = 5,
                                        .stack_size = 4096,
                                        .core_id = 0,
                                        .callback = msc_callback};
  e = msc_host_install(&msc);
  if (e != ESP_OK) {
    usb_host_uninstall();
    snprintf(error_text, sizeof(error_text), "MSC driver: %s",
             esp_err_to_name(e));
    return false;
  }
  if (xTaskCreatePinnedToCore(usb_daemon, "usb_host", 4096, NULL, 4, NULL, 0) !=
      pdPASS) {
    msc_host_uninstall();
    usb_host_uninstall();
    snprintf(error_text, sizeof(error_text), "Not enough memory for USB host");
    return false;
  }
  return true;
}
static bool is_zip(const char *name) {
  size_t n = strlen(name);
  return n > 4 && name[n - 4] == '.' &&
         tolower((unsigned char)name[n - 3]) == 'z' &&
         tolower((unsigned char)name[n - 2]) == 'i' &&
         tolower((unsigned char)name[n - 1]) == 'p';
}
static void scan(void) {
  count = selected = skipped = 0;
  last_rejection[0] = 0;
  base("SCANNING USB DRIVE");
  text(24, 100, "LOOKING FOR FRESLEST ZIP FILES", 2, TEXT);
  show();
  DIR *dir = opendir(USB_ROOT);
  if (!dir) {
    snprintf(last_rejection, sizeof(last_rejection), "Cannot read USB root");
    draw();
    return;
  }
  struct dirent *ent;
  unsigned examined = 0;
  while ((ent = readdir(dir)) && count < MAX_PACKAGES && examined < 128 &&
         !atomic_load(&removed)) {
    if (!is_zip(ent->d_name))
      continue;
    ++examined;
    char path[160];
    int length = snprintf(path, sizeof(path), USB_ROOT "/%s", ent->d_name);
    if (length < 0 || length >= (int)sizeof(path) ||
        strlen(ent->d_name) >= sizeof(packages[0].filename)) {
      ++skipped;
      continue;
    }
    struct stat st;
    if (stat(path, &st) || !S_ISREG(st.st_mode))
      continue;
    FILE *file = fopen(path, "rb");
    if (!file) {
      ++skipped;
      continue;
    }
    freslest_update_package p;
    bool ok = freslest_update_inspect(file, &p, last_rejection,
                                      sizeof(last_rejection));
    fclose(file);
    if (ok) {
      snprintf(packages[count].filename, sizeof(packages[count].filename),
               "%.127s", ent->d_name);
      packages[count++].package = p;
    } else
      ++skipped;
    vTaskDelay(1);
  }
  closedir(dir);
  draw();
}
static void connect_drive(uint8_t address) {
  if (!address || !host_ready)
    return;
  last_usb_address = address;
  unmount_drive();
  atomic_store(&removed, false);
  failed = confirm = false;
  esp_err_t e = msc_host_install_device(address, &drive);
  if (e != ESP_OK) {
    snprintf(error_text, sizeof(error_text), "E301 USB device init: %s",
             esp_err_to_name(e));
    unmount_drive();
    draw();
    return;
  }
  msc_host_device_info_t info = {0};
  if (msc_host_get_device_info(drive, &info) == ESP_OK)
    ESP_LOGI(TAG, "USB %04x:%04x sector=%lu count=%lu", info.idVendor,
             info.idProduct, (unsigned long)info.sector_size,
             (unsigned long)info.sector_count);
  const esp_vfs_fat_mount_config_t cfg = {.format_if_mount_failed = false,
                                          .max_files = 3,
                                          .allocation_unit_size = 0};
  e = msc_host_vfs_register(drive, USB_ROOT, &cfg, &mounted);
  if (e != ESP_OK || !mounted) {
    snprintf(error_text, sizeof(error_text), "E302 FAT mount: %s / FR=%d",
             esp_err_to_name(e), msc_host_vfs_get_last_mount_result());
    ESP_LOGE(TAG, "%s", error_text);
    unmount_drive();
    draw();
    return;
  }
  error_text[0] = 0;
  scan();
}
static void check_progress(unsigned percent, void *unused) {
  (void)unused;
  progress((percent * 25) / 100, "Checking ZIP and firmware");
}
static bool install(void) {
  const esp_partition_t *target = os_partition();
  if (!target)
    return snprintf(error_text, sizeof(error_text),
                    "Unexpected flash layout. Use the PC installer."),
           false;
  candidate *choice = &packages[selected];
  uint8_t *image = heap_caps_malloc(choice->package.image_size,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!image)
    return snprintf(error_text, sizeof(error_text),
                    "Not enough PSRAM to check the update"),
           false;
  char path[160];
  snprintf(path, sizeof(path), USB_ROOT "/%s", choice->filename);
  FILE *file = fopen(path, "rb");
  bool ok = false;
  last_percent = -1;
  progress(0, "Checking ZIP and firmware");
  if (file) {
    // Re-inspect: the USB file may have changed since it was listed.
    freslest_update_package current;
    ok =
        freslest_update_inspect(file, &current, error_text, sizeof(error_text));
    if (ok && (strcmp(current.version, choice->package.version) ||
               current.image_size != choice->package.image_size ||
               memcmp(current.sha256, choice->package.sha256, 32)))
      ok = false, snprintf(error_text, sizeof(error_text),
                           "ZIP changed. Rescan and confirm again.");
    if (ok)
      ok = freslest_update_load(file, &current, image,
                                choice->package.image_size, check_progress,
                                NULL, error_text, sizeof(error_text));
    fclose(file);
  } else
    snprintf(error_text, sizeof(error_text),
             "Cannot open update ZIP. Reinsert USB.");
  if (!ok) {
    heap_caps_free(image);
    return false;
  }
#ifndef FRESLEST_UPDATE_RUNTIME_TEST
  const esp_image_header_t *header = (const esp_image_header_t *)image;
  const esp_app_desc_t *description = (const esp_app_desc_t *)(image + 32);
  if (bootloader_common_check_chip_validity(header, ESP_IMAGE_APPLICATION) !=
          ESP_OK ||
      bootloader_common_check_efuse_blk_validity(
          description->min_efuse_blk_rev_full,
          description->max_efuse_blk_rev_full) != ESP_OK) {
    heap_caps_free(image);
    snprintf(error_text, sizeof(error_text),
             "Firmware needs a different chip revision.");
    return false;
  }
#endif
  // All compressed data, CRC, SHA256, ESP segments, ESP digest and project
  // identity have passed before the first flash erase. Stage the entire OS
  // in PSRAM, so unplugging the USB cannot interrupt a flash write.
  if (freslest_ota_begin_update(target) != ESP_OK) {
    heap_caps_free(image);
    snprintf(error_text, sizeof(error_text),
             "Cannot save OTA state. Current OS kept.");
    return false;
  }
  previous = NULL;
  uint32_t image_size = choice->package.image_size;
  unmount_drive();
  progress(26, "Writing the inactive OS slot");
  esp_ota_handle_t handle = 0;
  esp_err_t result = esp_ota_begin(target, image_size, &handle);
  ok = result == ESP_OK;
  if (!ok)
    snprintf(error_text, sizeof(error_text), "OTA begin: %s",
             esp_err_to_name(result));
  for (uint32_t offset = 0; ok && offset < image_size; offset += 4096) {
    size_t bytes = image_size - offset;
    if (bytes > 4096)
      bytes = 4096;
    result = esp_ota_write(handle, image + offset, bytes);
    if (result != ESP_OK) {
      snprintf(error_text, sizeof(error_text), "OTA write: %s",
               esp_err_to_name(result));
      ok = false;
    } else
      progress(26 + (unsigned)((uint64_t)(offset + bytes) * 68 / image_size),
               "Writing the inactive OS slot");
    vTaskDelay(1);
  }
  if (ok) {
    result = esp_ota_end(handle);
    handle = 0;
    ok = result == ESP_OK;
    if (!ok)
      snprintf(error_text, sizeof(error_text), "OTA image rejected: %s",
               esp_err_to_name(result));
  } else if (handle)
    esp_ota_abort(handle);
  uint8_t hash[32];
  if (ok) {
    progress(95, "Verifying written firmware");
    // esp_partition_get_sha256(app) verifies the ESP image, but returns
    // the image's embedded digest, not the ZIP's whole-file hash. Use a
    // separate streaming read-back hash to check every written byte.
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    ok = mbedtls_sha256_starts(&sha, 0) == 0;
    uint8_t block[4096];
    for (uint32_t offset = 0; ok && offset < image_size;
         offset += sizeof(block)) {
      size_t bytes = image_size - offset;
      if (bytes > sizeof(block))
        bytes = sizeof(block);
      ok = esp_partition_read(target, offset, block, bytes) == ESP_OK &&
           mbedtls_sha256_update(&sha, block, bytes) == 0;
      vTaskDelay(1);
    }
    if (ok)
      ok = mbedtls_sha256_finish(&sha, hash) == 0;
    mbedtls_sha256_free(&sha);
    ok = ok && !memcmp(hash, choice->package.sha256, 32);
    if (!ok)
      snprintf(error_text, sizeof(error_text),
               "Written firmware checksum failed. Retry USB.");
  }
  heap_caps_free(image);
  if (!ok) {
    freslest_ota_cancel_update();
    return false;
  }
  if (ok) {
    progress(99, "Checking boot image");
    esp_err_t e = freslest_ota_select_update(
        target); // IDF validates before selecting the trial image.
    ok = e == ESP_OK;
    if (!ok)
      snprintf(error_text, sizeof(error_text), "Boot image rejected: %s",
               esp_err_to_name(e));
  }
  if (ok) {
    progress(100, "Restarting. First boot will be checked.");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
  }
  return false;
}
typedef struct {
  int pin;
  bool raw, down, edge;
  int64_t changed;
} key;
static key keys[] = {{.pin = BTN_UP},
                     {.pin = BTN_DOWN},
                     {.pin = BTN_OK},
                     {.pin = BTN_BACK},
                     {.pin = BTN_FIRE}};
#define KEY_COUNT (sizeof(keys) / sizeof(keys[0]))
static void keys_init(void) {
  uint64_t mask = 0;
  for (unsigned i = 0; i < KEY_COUNT; i++)
    mask |= 1ull << keys[i].pin;
  gpio_config_t c = {.pin_bit_mask = mask,
                     .mode = GPIO_MODE_INPUT,
                     .pull_up_en = GPIO_PULLUP_ENABLE};
  gpio_config(&c);
  for (unsigned i = 0; i < KEY_COUNT; i++)
    keys[i].down = keys[i].raw = !gpio_get_level(keys[i].pin);
}
static void keys_poll(void) {
  int64_t now = esp_timer_get_time();
  for (unsigned i = 0; i < KEY_COUNT; i++) {
    key *k = &keys[i];
    k->edge = false;
    bool down = !gpio_get_level(k->pin);
    if (k->raw != down) {
      k->raw = down;
      k->changed = now;
    }
    if (k->down != down && now - k->changed >= 25000) {
      k->down = down;
      k->edge = down;
    }
  }
}
static void handle_keys(void) {
  if (keys[3].edge) {
    return_to_os();
    return;
  }
  if (restore_confirm) {
    if (keys[2].edge) {
      esp_err_t e = freslest_ota_restore_previous();
      if (e == ESP_OK) {
        unmount_drive();
        esp_restart();
      }
      snprintf(error_text, sizeof(error_text), "Previous OS: %s",
               esp_err_to_name(e));
      restore_confirm = false;
      failed = true;
      draw();
    }
    return;
  }
  if (keys[4].edge && previous) {
    confirm = false;
    failed = false;
    restore_confirm = true;
    draw();
    return;
  }
  if (failed) {
    if (keys[2].edge) {
      failed = false;
      confirm = false;
      if (mounted)
        scan();
      else if (last_usb_address && !atomic_load(&removed))
        connect_drive(last_usb_address);
      else
        draw();
    }
    return;
  }
  if (confirm) {
    if (keys[2].edge) {
      confirm = false;
      if (!install()) {
        failed = true;
        draw();
      }
    }
  } else if (count) {
    if (keys[0].edge) {
      selected = (selected + count - 1) % count;
      draw();
    }
    if (keys[1].edge) {
      selected = (selected + 1) % count;
      draw();
    }
    if (keys[2].edge) {
      confirm = true;
      draw();
    }
  } else if (keys[2].edge) {
    if (mounted)
      scan();
    else if (last_usb_address && !atomic_load(&removed))
      connect_drive(last_usb_address);
  }
}
void freslest_update_task(void *argument) {
  (void)argument;
  screen = heap_caps_malloc(480 * 320, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!screen) {
    ESP_LOGE(TAG, "Cannot allocate updater screen");
    vTaskDelete(NULL);
    return;
  }
  previous = freslest_ota_previous_os();
  if (previous) {
    esp_app_desc_t d;
    if (esp_ota_get_partition_description(previous, &d) == ESP_OK)
      snprintf(previous_version, sizeof(previous_version), "%s", d.version);
  }
  keys_init();
  host_ready = start_host();
  if (!host_ready && !error_text[0])
    snprintf(error_text, sizeof(error_text),
             "Cannot start USB host. Press BACK.");
  draw();
  for (;;) {
    keys_poll();
    msc_host_event_t event;
    while (usb_events && xQueueReceive(usb_events, &event, 0) == pdTRUE) {
      if (event.event == MSC_DEVICE_DISCONNECTED) {
        if (drive && event.device.handle == drive) {
          unmount_drive();
          failed = false;
          error_text[0] = 0;
          draw();
        }
      } else if (event.event == MSC_DEVICE_CONNECTED && !drive && host_ready) {
        connect_drive(event.device.address);
      }
    }
    handle_keys();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
