#include "doom_save_export.h"
#include "nes_usb_disk.h"
#include "audio_manager.h"
#include <Arduino.h>
#include <algorithm>
#include <esp_heap_caps.h>
#include <freslest_save.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace {
void slotName(int slot, char (&name)[13]) {
    memcpy(name, "prbmsav0.dsg", sizeof(name));
    name[7] = '0' + slot;
}

bool backupFolder(const char *root, char (&folder)[48]) {
    snprintf(folder, sizeof(folder), "%s/DOOM", root);
    if (mkdir(folder, 0777) != 0 && errno != EEXIST) return false;
    for (unsigned n = 1; n <= 999; ++n) {
        snprintf(folder, sizeof(folder), "%s/DOOM/BACKUP%03u", root, n);
        if (mkdir(folder, 0777) == 0) return true;
        if (errno != EEXIST) break;
    }
    folder[0] = 0;
    return false;
}
} // namespace

DoomSaveExport doomSaveExportDisk(uint8_t *disk, size_t capacity) {
    DoomSaveExport result{};
    for (int slot = 0; slot < FRESLEST_SAVE_SLOTS; ++slot) {
        char path[13], shortName[] = "PRBMSAV0DSG";
        slotName(slot, path);
        int size = freslest_save_size(path);
        if (size <= 0) continue;
        shortName[7] = '0' + slot;
        if (NesUsbDisk::appendFile(disk, capacity, shortName, size, freslest_save_read, path))
            ++result.copied;
        else ++result.failed;
        delay(1);
    }
    return result;
}

DoomSaveExport doomSaveExportFiles(const char *root, char (&folder)[48]) {
    DoomSaveExport result{};
    int sizes[FRESLEST_SAVE_SLOTS];
    size_t capacity = 0;
    for (int slot = 0; slot < FRESLEST_SAVE_SLOTS; ++slot) {
        char name[13]; slotName(slot, name);
        sizes[slot] = freslest_save_size(name);
        if (sizes[slot] > static_cast<int>(capacity)) capacity = sizes[slot];
    }
    folder[0] = 0;
    if (!capacity) return result;
    auto *data = static_cast<uint8_t *>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data) return {0, 1};
    if (!backupFolder(root, folder)) { heap_caps_free(data); return {0, 1}; }
    char pending[64];
    snprintf(pending, sizeof(pending), "%s/SAVE.TMP", folder);
    for (int slot = 0; slot < FRESLEST_SAVE_SLOTS; ++slot) {
        const int size = sizes[slot];
        if (size <= 0) continue;
        char name[13], output[64];
        slotName(slot, name);
        snprintf(output, sizeof(output), "%s/%s", folder, name);
        bool ok = freslest_save_read(name, data, size);
        FILE *file = ok ? fopen(pending, "wb") : nullptr;
        ok = file != nullptr;
        if (file) {
            for (size_t offset = 0; ok && offset < static_cast<size_t>(size);) {
                const size_t count = std::min<size_t>(4096, size - offset);
                ok = fwrite(data + offset, 1, count, file) == count;
                offset += count;
                audioUpdate(millis());
                delay(1);
            }
            ok = ok && fflush(file) == 0 && fsync(fileno(file)) == 0;
            if (fclose(file) != 0) ok = false;
        }
        if (ok && rename(pending, output) == 0) ++result.copied;
        else { unlink(pending); ++result.failed; }
    }
    heap_caps_free(data);
    if (!result.copied) { rmdir(folder); folder[0] = 0; }
    return result;
}
