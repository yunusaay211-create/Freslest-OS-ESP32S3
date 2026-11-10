#include "frestore.h"
#include "frestore_package.h"
#include "frestore_starter.h"
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "apps.h"
#include "storage_init.h"
#include "system_status.h"
#include "text_entry.h"
#include "usb_storage.h"
#include <WiFi.h>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <esp_attr.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <esp_littlefs.h>
#include <esp_vfs_fat.h>
#include <mbedtls/sha256.h>
#include <nvs.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using namespace frestore;
FreStoreTrust trust{CONFIG_FRESLEST_STORE_ORIGIN, CONFIG_FRESLEST_STORE_PUBLIC_KEY};
FreStoreTrust candidate{};
char keyFingerprint[17];
bool trustLoaded = false, useStarter = true;
char publisherId[17];

enum class Job { Scan, Catalog, Install, Remove, Open, Pair, Import };
enum class View { List, Detail, Card, Servers, Trust, Destination };
enum class Edit { None, Search, Address };
enum class Storage : uint8_t { Flash, Sd, Usb };
constexpr const char *destinations[] = {"Internal flash", "SD card", "USB drive"};
struct Installed { char id[25], name[35]; uint32_t version; Storage storage; bool legacy; };
struct Selection { App offer; Installed local; };
EXT_RAM_BSS_ATTR App offers[MaxApps];
Installed *installed = nullptr;
size_t installedCapacity = 0;
EXT_RAM_BSS_ATTR char download[CatalogBytes], fileData[PackageBytes];
EXT_RAM_BSS_ATTR Card card, parsedCard;
unsigned offerCount = 0, installedCount = 0;
uint32_t diskRevision = UINT32_MAX;
Selection target{};
std::atomic<bool> cancelled{false}, completed{false};
std::atomic<unsigned> percent{0};
bool working = false; // Only the UI task starts jobs and consumes completions.
const char *message = "";
Job job;
View view = View::List;
Edit editing = Edit::None;
bool installedOnly;
int selected, category, offset, cardTop, serverRow, destinationRow;
unsigned shownProgress = 101;
char query[35] = "";
constexpr const char *categories[] = {"All categories", "Tools", "Education", "Media"};
constexpr const char *serverOptions[] = {
    "Starter catalog", "Saved server", "Add HTTPS server", "Import previous SD settings"
};

class UsbAccess {
    const bool usb;
public:
    const bool ok;
    explicit UsbAccess(bool needed) : usb(needed), ok(!usb || usbStorageLock()) {}
    ~UsbAccess() { if (usb && ok) usbStorageUnlock(); }
};

const char *legacyRoot() { return freslestSdMounted() ? freslestSdRoot() : usbStorageRoot(); }
const char *storageRoot(Storage storage) {
    if (storage == Storage::Sd) return freslestSdRoot();
    if (storage == Storage::Usb) return usbStorageRoot();
    return "/littlefs";
}
bool available(Storage storage) {
    if (storage == Storage::Sd) return freslestSdMounted();
    if (storage == Storage::Usb) return usbStorageInfo().state == UsbStorageState::Ready;
    return true;
}
void appDirectory(char (&out)[48], Storage storage, bool legacy = false) {
    snprintf(out, sizeof(out), "%s/FRESTORE%s%s", storageRoot(storage), legacy ? "" : "/",
             legacy ? "" : publisherId);
}

bool validOrigin(const char *origin) {
    if (strncmp(origin, "https://", 8) || !origin[8]) return false;
    for (const char *p = origin + 8; *p; ++p)
        if (!isalnum(static_cast<unsigned char>(*p)) && *p != '.' && *p != '-' && *p != ':') return false;
    return true;
}
bool configured() { return trust.key[0] && validOrigin(trust.origin); }
const FreStoreTrust &activeStore() { return useStarter ? FreStoreStarter : trust; }
void activateStore(bool starter) {
    useStarter = starter || !configured();
    publisherId[0] = 0;
    fingerprint(activeStore().key, publisherId);
    offerCount = installedCount = 0;
    diskRevision = UINT32_MAX;
}
void loadTrust() {
    if (trustLoaded) return;
    trustLoaded = true;
    nvs_handle_t nvs;
    if (!configured() || !fingerprint(trust.key, keyFingerprint)) trust = {};
    uint8_t starter = 0;
    if (nvs_open("fl_store", NVS_READONLY, &nvs) == ESP_OK) {
        size_t size = sizeof(candidate);
        if (nvs_get_blob(nvs, "trust_v1", &candidate, &size) == ESP_OK && size == sizeof(candidate) &&
            !candidate.origin[sizeof(candidate.origin) - 1] && !candidate.key[sizeof(candidate.key) - 1] &&
            validOrigin(candidate.origin) && fingerprint(candidate.key, keyFingerprint)) trust = candidate;
        nvs_get_u8(nvs, "use_starter", &starter);
        nvs_close(nvs);
    }
    activateStore(starter != 0);
}
bool saveSelection(bool starter, const FreStoreTrust *replacement = nullptr) {
    nvs_handle_t nvs;
    if (nvs_open("fl_store", NVS_READWRITE, &nvs) != ESP_OK) return false;
    const bool saved = (!replacement || nvs_set_blob(nvs, "trust_v1", replacement, sizeof(trust)) == ESP_OK) &&
                       nvs_set_u8(nvs, "use_starter", starter) == ESP_OK && nvs_commit(nvs) == ESP_OK;
    nvs_close(nvs);
    if (saved) {
        if (replacement) trust = *replacement;
        activateStore(starter);
    }
    return saved;
}
bool fetch(const char *origin, const char *path, size_t capacity, size_t &length,
           FILE *output = nullptr, const uint8_t *expectedHash = nullptr) {
    length = 0;
    if (WiFi.status() != WL_CONNECTED) { message = "Connect to Wi-Fi first."; return false; }
    struct tm time{};
    if (!rtcGetLocal(time)) { message = "Waiting for the clock to sync for HTTPS."; return false; }
    char url[256];
    int n = snprintf(url, sizeof(url), "%s%s", origin, path);
    if (n < 0 || n >= (int)sizeof(url)) { message = "Invalid store URL."; return false; }
    esp_http_client_config_t config{};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.disable_auto_redirect = true;
    config.timeout_ms = 4000;
    config.buffer_size = 1024;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    message = "HTTPS connection failed.";
    if (!client) return false;
    bool ok = false;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        const int64_t bytes = esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (status != 200) message = status == 404 ? "Content was not found on the server." : "Server request failed.";
        else if (bytes <= 0 || bytes > (int64_t)capacity || (output && bytes != (int64_t)capacity) ||
                 esp_http_client_is_chunked_response(client))
            message = "Invalid file size from the server.";
        else {
            const int64_t deadline = esp_timer_get_time() + 25000000;
            mbedtls_sha256_context sha;
            mbedtls_sha256_init(&sha);
            bool wrote = !output || mbedtls_sha256_starts(&sha, 0) == 0;
            while (wrote && !cancelled.load() && length < (size_t)bytes &&
                   (output || esp_timer_get_time() < deadline)) {
                char *chunk = output ? download : download + length;
                int count = esp_http_client_read(client, chunk, std::min<size_t>(4096, bytes - length));
                if (count <= 0) break;
                if (output)
                    wrote = fwrite(chunk, 1, count, output) == (size_t)count &&
                            mbedtls_sha256_update(&sha, reinterpret_cast<uint8_t *>(chunk), count) == 0;
                length += count;
                percent.store((uint64_t)length * 100 / bytes);
            }
            ok = wrote && !cancelled.load() && length == (size_t)bytes &&
                 esp_http_client_is_complete_data_received(client);
            if (!ok) message = cancelled.load() ? "Download cancelled." :
                              wrote ? "Download interrupted. Try again." : "Disk write failed. Check free space.";
            if (ok && output) {
                uint8_t hash[32];
                ok = mbedtls_sha256_finish(&sha, hash) == 0 && !memcmp(hash, expectedHash, sizeof(hash));
                if (!ok) message = "Invalid downloaded asset hash.";
            }
            mbedtls_sha256_free(&sha);
        }
    }
    esp_http_client_cleanup(client);
    return ok;
}

void path(char (&out)[80], const char *directory, const char *id, char slot, bool asset = false) {
    snprintf(out, sizeof(out), "%s/%s.%c%s", directory, id, slot, asset ? ".dat" : "");
}
bool readSlot(const char *directory, const char *id, char slot, Card &out) {
    char file[80]; path(file, directory, id, slot);
    FILE *f = fopen(file, "rb");
    if (!f) return false;
    size_t n = fread(fileData, 1, sizeof(fileData), f);
    bool complete = !ferror(f) && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    if (!complete || !parseCard(fileData, n, activeStore().key, out) || strcmp(out.id, id)) return false;
    if (!out.assetSize) return true;
    struct stat st{};
    path(file, directory, id, slot, true);
    return stat(file, &st) == 0 && S_ISREG(st.st_mode) &&
           static_cast<uint32_t>(st.st_size) == out.assetSize;
}
// The previous verified slot stays available while the next one is written.
char newest(const char *directory, const char *id, Card &out) {
    const bool a = readSlot(directory, id, 'a', out);
    const bool b = readSlot(directory, id, 'b', parsedCard);
    if (b && (!a || parsedCard.version > out.version)) { out = parsedCard; return 'b'; }
    return a ? 'a' : '\0';
}
int localIndex(const char *id) {
    int best = -1;
    for (unsigned i = 0; i < installedCount; ++i)
        if (!strcmp(installed[i].id, id) && (best < 0 || installed[i].version > installed[best].version))
            best = i;
    return best;
}
bool scanDirectory(Storage storage, bool legacy = false) {
    char directory[48]; appDirectory(directory, storage, legacy);
    DIR *dir = opendir(directory);
    if (!dir) return true;
    bool complete = true;
    while (const dirent *entry = readdir(dir)) {
        const size_t n = strlen(entry->d_name);
        if (n < 3 || n > 26 || entry->d_name[n-2] != '.') continue;
        const char slot = tolower(static_cast<unsigned char>(entry->d_name[n-1]));
        if (slot != 'a' && slot != 'b') continue;
        char id[25];
        for (size_t i = 0; i < n - 2; ++i) id[i] = tolower(static_cast<unsigned char>(entry->d_name[i]));
        id[n-2] = 0;
        if (!validId(id)) continue;
        vTaskDelay(1);
        if (newest(directory, id, card) != slot) continue;
        if (installedCount == installedCapacity) {
            const size_t next = installedCapacity ? installedCapacity * 2 : MaxApps;
            auto *list = static_cast<Installed *>(heap_caps_realloc(
                installed, next * sizeof(Installed), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (!list) { complete = false; break; }
            installed = list;
            installedCapacity = next;
        }
        auto &item = installed[installedCount++];
        strcpy(item.id, card.id);
        strcpy(item.name, card.name);
        item.version = card.version;
        item.storage = storage;
        item.legacy = legacy;
    }
    closedir(dir);
    return complete;
}
bool scan() {
    installedCount = 0;
    if (!publisherId[0]) return true;
    if (!scanDirectory(Storage::Flash)) return false;
    if (freslestSdMounted()) {
        if (!scanDirectory(Storage::Sd) || (!useStarter && !scanDirectory(Storage::Sd, true))) return false;
    }
    if (available(Storage::Usb)) {
        UsbAccess disk(true);
        if (disk.ok) {
            if (!scanDirectory(Storage::Usb) || (!useStarter && !scanDirectory(Storage::Usb, true))) return false;
        }
    }
    return true;
}
bool removeFile(const char *file) { return unlink(file) == 0 || errno == ENOENT; }
bool makeDirectory(const char *directory) { return mkdir(directory, 0777) == 0 || errno == EEXIST; }
bool finishFile(FILE *file, bool ok) {
    ok = ok && fflush(file) == 0 && fsync(fileno(file)) == 0;
    return fclose(file) == 0 && ok;
}
bool freeSpace(Storage storage, uint64_t &free) {
    if (storage == Storage::Flash) {
        size_t total, used;
        if (esp_littlefs_info("spiffs", &total, &used) != ESP_OK || used > total) return false;
        free = total - used;
        return true;
    }
    uint64_t total;
    return esp_vfs_fat_info(storageRoot(storage), &total, &free) == ESP_OK;
}
void install() {
    const App &offer = target.offer;
    const auto storage = static_cast<Storage>(destinationRow);
    UsbAccess disk(storage == Storage::Usb);
    const uint64_t needed = (uint64_t)offer.size + offer.assetSize;
    if (!disk.ok || !available(storage) || !publisherId[0]) {
        message = "Selected storage is unavailable.";
        return;
    }
    if (storage == Storage::Flash && needed > InternalPackageLimit) {
        message = "Internal flash limit: 500 KiB per package. Choose SD or USB.";
        return;
    }
    char directory[48], root[48]; appDirectory(directory, storage); appDirectory(root, storage, true);
    const char current = newest(directory, offer.id, card);
    if (current && card.version >= offer.version) { message = "Already installed on this storage."; return; }
    if (!makeDirectory(root) || !makeDirectory(directory)) {
        message = "Cannot create the app folder on this storage.";
        return;
    }
    char pending[80], pendingData[80], next[80], nextData[80];
    const char nextSlot = current == 'a' ? 'b' : 'a';
    path(pending, directory, offer.id, 't');
    path(pendingData, directory, offer.id, 't', true);
    path(next, directory, offer.id, nextSlot);
    path(nextData, directory, offer.id, nextSlot, true);
    // FAT rename requires a free destination. Only discard inactive or temporary files.
    if (!removeFile(pending) || !removeFile(pendingData) || !removeFile(next) || !removeFile(nextData)) {
        message = "Cannot prepare selected storage.";
        return;
    }
    uint64_t free = 0;
    if (!freeSpace(storage, free) || free < needed + (storage == Storage::Flash ? 4096 : 0)) {
        message = "Not enough free space on the selected storage.";
        return;
    }
    char route[80];
    snprintf(route, sizeof(route), "/v1/packages/%s/%lu", offer.id, (unsigned long)offer.version);
    size_t length;
    if (!fetch(activeStore().origin, route, offer.size, length)) return;
    if (!matches(download, length, offer) || !parseCard(download, length, activeStore().key, card) ||
        strcmp(offer.id, card.id) || strcmp(offer.name, card.name) || offer.version != card.version ||
        offer.assetSize != card.assetSize) {
        message = "Invalid package signature, hash or ID.";
        return;
    }
    if (cancelled.load()) { message = "Installation cancelled."; return; }
    FILE *f = fopen(pending, "wb");
    bool saved = f && finishFile(f, fwrite(download, 1, length, f) == length);
    message = "Cannot write to the selected storage.";
    if (saved && offer.assetSize) {
        FILE *asset = fopen(pendingData, "wb");
        saved = false;
        if (asset) {
            strcat(route, "/asset");
            percent.store(0);
            bool downloaded = fetch(activeStore().origin, route, offer.assetSize, length, asset, card.assetDigest);
            saved = finishFile(asset, downloaded) && !cancelled.load() && rename(pendingData, nextData) == 0;
            if (downloaded) message = "Cannot finish writing the app to the selected storage.";
        }
    }
    saved = saved && !cancelled.load() && rename(pending, next) == 0;
    if (!saved) {
        removeFile(pending);
        removeFile(pendingData);
        removeFile(nextData);
        if (cancelled.load()) message = "Installation cancelled.";
    } else {
        constexpr const char *success[] = {"Installed to internal flash.", "Installed to SD card.",
                                           "Installed to USB drive."};
        message = success[destinationRow];
    }
}
bool importTrust() {
    char filename[48];
    snprintf(filename, sizeof(filename), "%s/FRESTORE.CFG", legacyRoot());
    FILE *f = fopen(filename, "r");
    if (!f) return false;
    candidate = {};
    bool read = fgets(candidate.origin, sizeof(candidate.origin), f) &&
                fgets(candidate.key, sizeof(candidate.key), f) && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    char *end = strpbrk(candidate.origin, "\r\n");
    if (!read || !end) return false;
    *end = 0;
    if (char *keyEnd = strpbrk(candidate.key, "\r\n")) *keyEnd = 0;
    return validOrigin(candidate.origin) && fingerprint(candidate.key, keyFingerprint);
}
void requestPublisher() {
    size_t length;
    if (!fetch(candidate.origin, "/v1/publisher.pub", sizeof(candidate.key) - 1, length)) return;
    while (length && (download[length - 1] == '\r' || download[length - 1] == '\n')) --length;
    if (!length || memchr(download, 0, length)) { message = "Invalid publisher key."; return; }
    memcpy(candidate.key, download, length);
    candidate.key[length] = 0;
    message = fingerprint(candidate.key, keyFingerprint) ? "" : "Invalid publisher key.";
}
void worker(void *) {
    if (job == Job::Catalog) {
        scan();
        size_t length;
        if (fetch(activeStore().origin, "/v1/catalog", CatalogBytes, length))
            message = parseCatalog(download, length, activeStore().key, offers, offerCount) ?
                      "Catalog verified." : "Invalid catalog signature or format.";
    } else if (job == Job::Install) {
        install();
    } else if (job == Job::Scan) {
        message = !scan() ? "Not enough PSRAM to list all installed apps." :
                  installedCount ? "Installed apps loaded." : "No installed apps yet.";
    } else if (job == Job::Pair) {
        requestPublisher();
    } else if (job == Job::Import) {
        UsbAccess disk(!freslestSdMounted());
        message = disk.ok && importTrust() ? "" : "No valid previous settings on SD or USB.";
    } else {
        UsbAccess disk(target.local.storage == Storage::Usb);
        char directory[48]; appDirectory(directory, target.local.storage, target.local.legacy);
        if (!disk.ok || !available(target.local.storage)) {
            message = "Reconnect the storage containing this app.";
        } else if (job == Job::Open) {
            message = newest(directory, target.local.id, card) ? "" : "Installed package is missing or invalid.";
        } else {
            char file[80];
            const char current = newest(directory, target.local.id, card);
            bool removed = true;
            for (const char slot : {'t', current == 'a' ? 'b' : 'a', current == 'a' ? 'a' : 'b'}) {
                path(file, directory, target.local.id, slot);
                if (!removeFile(file)) { removed = false; break; }
                path(file, directory, target.local.id, slot, true);
                if (!removeFile(file)) { removed = false; break; }
            }
            message = removed ? "App removed." : "Cannot remove the app.";
        }
    }
    if (job == Job::Remove || job == Job::Install) scan();
    completed.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
}
void start(Job next) {
    if (working) return;
    job = next;
    working = true;
    completed.store(false);
    cancelled.store(false);
    percent.store(0);
    shownProgress = 101;
    if (xTaskCreatePinnedToCore(worker, "frestore", 10240, nullptr, 2, nullptr, 0) != pdPASS) {
        working = false;
        message = "Not enough memory for FreStore.";
    }
}

bool matchesQuery(const char *name) {
    if (!query[0]) return true;
    const size_t length = strlen(query);
    for (; *name; ++name) if (!strncasecmp(name, query, length)) return true;
    return false;
}
int listIndex(int row) {
    for (unsigned i = 0; i < (installedOnly ? installedCount : offerCount); ++i) {
        if (!installedOnly && category && category - 1 != offers[i].category) continue;
        if (!matchesQuery(installedOnly ? installed[i].name : offers[i].name)) continue;
        if (row-- == 0) return i;
    }
    return -1;
}
void label(int x, int y, const char *text) { tft.setCursor(x, y); tft.print(text); }
void button(int x, int y, int width, const char *title) {
    tft.fillRoundRect(x, y, width, 20, 3, TH().panel);
    label(x + 6, y + 6, title);
}
int rows() { return (tft.height() - 117) / 20; }
void wrap(int x, int y, const char *text) {
    const int columns = (tft.width() - 24) / 6;
    for (int i = 0, n = strlen(text); i < n; i += columns) {
        tft.setCursor(x, y + 12 * (i / columns));
        tft.printf("%.*s", columns, text + i);
    }
}
bool cancellable() { return job == Job::Catalog || job == Job::Install || job == Job::Pair; }
bool canOpen() { return installedOnly || target.local.version >= target.offer.version; }
void draw() {
    const int w = tft.width(), h = tft.height();
    tft.fillScreen(TH().bg);
    drawHeader(installedOnly ? "Installed Apps" : "FreStore");
    drawFooter("OK Select  FIRE Refresh", "BACK Close / Up");
    tft.setTextSize(1);
    tft.setTextColor(TH().text);
    if (working) {
        const unsigned progress = percent.load();
        shownProgress = progress;
        label(12, 80, job == Job::Scan ? "Reading installed apps..." : "Working...");
        if (cancellable()) {
            tft.drawRect(12, 100, w - 24, 14, TH().dim);
            tft.fillRect(14, 102, (w - 28) * progress / 100, 10, TH().accent);
            tft.setCursor(12, 124); tft.printf("%u%%", progress);
            button(12, 146, 84, "Cancel");
        }
        return;
    }
    if (view == View::Card) {
        label(12, 48, card.name);
        for (int i = 0; i < (h - 91) / 13 && i + cardTop < (int)card.count; ++i)
            label(12, 68 + i * 13, card.lines[i + cardTop]);
        return;
    }
    if (view == View::Destination) {
        label(12, 48, "Choose where to install");
        for (int i = 0; i < 3; ++i) {
            const int y = 68 + i * 27;
            tft.fillRoundRect(12, y, w - 24, 23, 3, destinationRow == i ? TH().selbg : TH().panel);
            label(19, y + 7, destinations[i]);
        }
        label(12, 152, "Internal flash: 500 KiB limit, subject to free space.");
    } else if (view == View::Servers) {
        for (int i = 0; i < 4; ++i) {
            const int y = 53 + i * 26;
            tft.fillRoundRect(12, y, w - 24, 22, 3, serverRow == i ? TH().selbg : TH().panel);
            label(19, y + 7, serverOptions[i]);
        }
    } else if (view == View::Trust) {
        label(12, 48, "Trust this server and publisher key?");
        wrap(12, 67, candidate.origin);
        tft.setCursor(12, 117); tft.printf("SHA-256 / first 16: %s", keyFingerprint);
        label(12, 137, "This key also authorizes Wi-Fi Update.");
        button(12, h - 76, 90, "Confirm");
    } else if (view == View::Detail) {
        const App &offer = target.offer;
        const auto &local = target.local;
        label(12, 48, installedOnly ? local.name : offer.name);
        tft.setCursor(12, 64);
        if (installedOnly) tft.printf("Version %lu   %s", (unsigned long)local.version,
                                      destinations[(unsigned)local.storage]);
        else {
            tft.printf("Version %lu  %lu B  ESP32-S3 / ABI 1",
                       (unsigned long)offer.version, (unsigned long)(offer.size + offer.assetSize));
            label(12, 80, offer.developer);
            wrap(12, 98, offer.description);
        }
        const char *action = canOpen() ? "Open" : (local.version ? "Update" : "Install");
        button(12, h - 76, 84, action);
        if (local.version) button(108, h - 76, 84, "Remove");
        if (!installedOnly && canOpen()) {
            button(204, h - 76, 96, "Install to...");
            drawFooter("OK Open  RIGHT Install to...", "BACK Close / Up");
        }
    } else {
        button(12, 40, 52, "Search");
        button(70, 40, 60, "Refresh");
        if (!installedOnly) {
            button(136, 40, 101, categories[category]);
            button(243, 40, 65, "Server");
        }
        if (query[0]) label(12, 64, query);
        for (int i = 0; i < rows(); ++i) {
            const int index = listIndex(offset + i);
            if (index < 0) break;
            const int y = 76 + i * 20;
            tft.fillRect(12, y, w - 24, 18, offset + i == selected ? TH().selbg : TH().panel);
            label(18, y + 5, installedOnly ? installed[index].name : offers[index].name);
            if (installedOnly) label(w - 92, y + 5, destinations[(unsigned)installed[index].storage]);
        }
    }
    if (message[0]) {
        tft.setCursor(12, h - 43);
        tft.printf("%.*s", (w - 24) / 6, message);
    }
}
void detail(int index) {
    target = {};
    if (installedOnly) target.local = installed[index];
    else {
        target.offer = offers[index];
        const int local = localIndex(target.offer.id);
        if (local >= 0) target.local = installed[local];
    }
    view = View::Detail;
}
} // namespace

bool frestoreReadTrust(FreStoreTrust &out) {
    loadTrust();
    if (!configured()) return false;
    out = trust;
    return true;
}

bool frestoreBusy() { return working && !completed.load(std::memory_order_acquire); }

void frestoreEnter(bool local) {
    loadTrust();
    installedOnly = local;
    view = View::List;
    selected = offset = category = 0;
    query[0] = 0;
    editing = Edit::None;
    if (local || (!useStarter && !freslestSdMounted())) usbStorageBegin();
    diskRevision = usbStorageInfo().revision;
    if (!working) {
        if (local) start(Job::Scan);
        else if (!offerCount) start(Job::Catalog);
    }
    draw();
}
void frestoreExit() {
    if (editing != Edit::None) textEntryCancel();
    editing = Edit::None;
}
void frestoreLoop() {
    if (working) {
        if (!completed.load(std::memory_order_acquire)) {
            if (cancellable() && (bBack.pressed() || usbInputHit(12, 146, 84, 20))) cancelled.store(true);
            if (shownProgress != percent.load()) draw();
            return;
        }
        working = false;
        if (job == Job::Open && view == View::Detail && !message[0]) { view = View::Card; cardTop = 0; }
        if ((job == Job::Pair || job == Job::Import) && !message[0] && !installedOnly) view = View::Trust;
        if (job == Job::Install || job == Job::Remove || job == Job::Catalog) {
            view = View::List;
            selected = offset = 0;
        }
        draw();
        return;
    }
    if (editing != Edit::None) {
        char entered[sizeof(candidate.origin)];
        const auto result = textEntryTakeResult(entered, sizeof(entered));
        if (result == TextEntryResult::None) return;
        if (result == TextEntryResult::Accepted) {
            if (editing == Edit::Search) {
                strlcpy(query, entered, sizeof(query));
                selected = offset = 0;
            } else if (validOrigin(entered)) {
                candidate = {};
                strlcpy(candidate.origin, entered, sizeof(candidate.origin));
                start(Job::Pair);
            } else message = "Enter an HTTPS address without a path.";
        }
        editing = Edit::None;
        draw();
        return;
    }
    if (view == View::List || (view == View::Detail && installedOnly)) {
        const auto disk = usbStorageInfo();
        if (disk.revision != diskRevision) {
            diskRevision = disk.revision;
            view = View::List;
            target.local = {};
            start(Job::Scan);
            draw();
            return;
        }
    }
    if (bBack.pressed()) {
        if (view == View::Card) view = View::Detail;
        else if (view == View::Trust) view = View::Servers;
        else if (view == View::Destination) view = View::Detail;
        else if (view == View::Detail || view == View::Servers) view = View::List;
        else { enterApp(APP_APPS); return; }
        draw();
        return;
    }
    const int step = bDown.pressed() || usbPointer.wheel < 0 ? 1 :
                     bUp.pressed() || usbPointer.wheel > 0 ? -1 : 0;
    const int h = tft.height();
    if (view == View::Trust) {
        if (bOk.pressed() || usbInputHit(12, h - 76, 90, 20)) {
            if (saveSelection(false, &candidate)) { frestoreEnter(false); return; }
            message = "Cannot save server settings.";
            draw();
        }
        return;
    }
    if (view == View::Servers) {
        if (step) serverRow = (serverRow + step + 4) % 4;
        bool choose = bOk.pressed();
        for (int i = 0; i < 4; ++i) {
            if (usbInputHit(12, 53 + i * 26, tft.width() - 24, 22)) {
                serverRow = i;
                choose = true;
                break;
            }
        }
        if (choose) {
            if (serverRow == 2) {
                textEntryBegin("HTTPS server address", configured() ? trust.origin : "https://",
                               sizeof(candidate.origin) - 1);
                editing = Edit::Address;
                return;
            }
            if (serverRow == 3) {
                if (!freslestSdMounted()) usbStorageBegin();
                start(Job::Import);
            } else if (serverRow == 1 && !configured()) {
                message = "Add an HTTPS server first.";
            } else if (saveSelection(serverRow == 0)) {
                frestoreEnter(false);
                return;
            } else message = "Cannot save server settings.";
        }
        if (step || choose) draw();
        return;
    }
    if (view == View::Card) {
        if (step) { cardTop = constrain(cardTop + step, 0, std::max(0, (int)card.count - (h - 91) / 13)); draw(); }
        return;
    }
    if (view == View::Destination) {
        if (step) destinationRow = (destinationRow + step + 3) % 3;
        bool choose = bOk.pressed();
        for (int i = 0; i < 3; ++i)
            if (usbInputHit(12, 68 + i * 27, tft.width() - 24, 23)) {
                destinationRow = i; choose = true; break;
            }
        if (choose) {
            const auto storage = static_cast<Storage>(destinationRow);
            if (storage == Storage::Usb) usbStorageBegin();
            if (!available(storage)) {
                message = storage == Storage::Sd ? "Insert an SD card and restart Freslest." :
                                                   "Connect a FAT USB drive and try again.";
            } else start(Job::Install);
        }
        if (step || choose) draw();
        return;
    }
    if (view == View::Detail) {
        const bool changeStorage = !installedOnly && canOpen() &&
            (bRight.pressed() || usbInputHit(204, h - 76, 96, 20));
        if (target.local.version && (bFire.pressed() || usbInputHit(108, h - 76, 84, 20))) start(Job::Remove);
        else if (changeStorage || bOk.pressed() || usbInputHit(12, h - 76, 84, 20)) {
            if (canOpen() && !changeStorage) start(Job::Open);
            else {
                view = View::Destination;
                destinationRow = 0;
                message = "";
            }
        } else return;
        draw();
        return;
    }
    if (usbInputHit(12, 40, 52, 20) || (bFire.pressed() && bRight.down())) {
        textEntryBegin("Search FreStore", query, sizeof(query) - 1);
        editing = Edit::Search;
        return;
    }
    if (!installedOnly && (usbInputHit(243, 40, 65, 20) ||
        (bFire.pressed() && bLeft.down()))) {
        serverRow = useStarter ? 0 : 1;
        view = View::Servers;
        message = "";
        draw(); return;
    }
    if (bFire.pressed() || usbInputHit(70, 40, 60, 20)) {
        start(installedOnly ? Job::Scan : Job::Catalog); draw(); return;
    }
    if (!installedOnly && (bLeft.pressed() || bRight.pressed() || usbInputHit(136, 40, 101, 20))) {
        category = (category + (bLeft.pressed() ? CategoryCount : 1)) % (CategoryCount + 1);
        selected = offset = 0;
        draw();
        return;
    }
    if (usbInputHit(12, 76, tft.width() - 24, rows() * 20)) {
        const int clicked = offset + (usbPointer.y - 76) / 20;
        const int index = listIndex(clicked);
        if (index >= 0) { selected = clicked; detail(index); draw(); }
        return;
    }
    if (bOk.pressed()) {
        const int index = listIndex(selected);
        if (index >= 0) { detail(index); draw(); }
    } else if (step && selected + step >= 0 && listIndex(selected + step) >= 0) {
        selected += step;
        if (selected < offset) offset = selected;
        if (selected >= offset + rows()) offset = selected - rows() + 1;
        draw();
    }
}
