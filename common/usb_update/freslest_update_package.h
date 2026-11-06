#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "../../OS/freslest/lcd_profile.h"

#if FRESLEST_LCD_MODEL == 9341
#define FRESLEST_UPDATE_BOARD "esp32s3-n16r8-ili9341-buttons"
#else
#define FRESLEST_UPDATE_BOARD "esp32s3-n16r8-ili9488-buttons"
#endif
#define FRESLEST_UPDATE_LAYOUT "freslest-s3-ab-v2"
#define FRESLEST_OS_SLOT_SIZE 0x1b0000u
#define FRESLEST_DOOM_SLOT_SIZE 0xe0000u
#define FRESLEST_UPDATE_MANIFEST "freslest-update.json"
#define FRESLEST_UPDATE_DOOM_MANIFEST "freslest-doom-update.json"

enum freslest_update_target {
    FRESLEST_UPDATE_OS = 0,
    FRESLEST_UPDATE_DOOM = 1,
};
enum freslest_update_display {
    FRESLEST_UPDATE_DISPLAY_LEGACY = 0,
    FRESLEST_UPDATE_DISPLAY_PRESERVE,
    FRESLEST_UPDATE_DISPLAY_LCD,
    FRESLEST_UPDATE_DISPLAY_VGA,
};

typedef struct {
    uint32_t local_offset, compressed_size, size, crc;
    uint16_t method, flags;
    char name[192];
} freslest_zip_entry;
typedef struct {
    enum freslest_update_target target;
    enum freslest_update_display display;
    char version[32];
    uint8_t sha256[32];
    uint32_t image_size;
    freslest_zip_entry image;
} freslest_update_package;
typedef void (*freslest_update_progress)(unsigned percent, void *context);

#ifdef __cplusplus
extern "C" {
#endif
// No ZIP extraction to a filesystem: only the manifest and specified image
// are read. All offsets, lengths, CRCs and the destination capacity are bounded.
bool freslest_update_inspect(FILE *file, freslest_update_package *package,
                            char *error, size_t error_size);
// Select a manifest explicitly; the original inspect API still prefers Doom.
// Without an output argument, board matching remains strict.
bool freslest_update_inspect_target(FILE *file, freslest_update_package *package,
                                   enum freslest_update_target target,
                                   char *error, size_t error_size);
// Runtime selection also checks explicit display tags against the active output.
// An untagged legacy manifest always retains strict LCD board matching.
bool freslest_update_inspect_target_for_output(FILE *file, freslest_update_package *package,
                                              enum freslest_update_target target, bool vga,
                                              char *error, size_t error_size);
bool freslest_update_load(FILE *file, const freslest_update_package *package,
                         uint8_t *image, size_t capacity,
                         freslest_update_progress progress, void *context,
                         char *error, size_t error_size);
bool freslest_update_validate_image(const uint8_t *image, size_t size,
                                    const freslest_update_package *package,
                                    char *error, size_t error_size);
#ifdef __cplusplus
}
#endif
