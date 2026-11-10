#pragma once
#include <stddef.h>
#include <stdint.h>

struct DoomSaveExport {
    unsigned copied, failed;
};

DoomSaveExport doomSaveExportDisk(uint8_t *disk, size_t capacity);
DoomSaveExport doomSaveExportFiles(const char *root, char (&folder)[48]);
