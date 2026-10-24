// Freslest Doom S3: 8 raw-flash save slots, no SD card required.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define FRESLEST_SAVE_SLOTS 8
#define FRESLEST_SAVE_SLOT_SIZE 0x40000u
#define FRESLEST_SAVE_HEADER_SIZE 0x1000u
#define FRESLEST_SAVE_MAX_SIZE (FRESLEST_SAVE_SLOT_SIZE - FRESLEST_SAVE_HEADER_SIZE)

int freslest_save_slot_from_path(const char *path);
int freslest_save_description(int slot, char *out, size_t cap);
int freslest_save_size(const char *path);
int freslest_save_read(const char *path, void *dst, size_t len);
int freslest_save_write(const char *path, const void *src, size_t len);
