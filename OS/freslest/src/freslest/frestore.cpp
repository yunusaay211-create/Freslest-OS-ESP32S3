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
#include <esp_http_client.h>
#include <esp_timer.h>
#include <esp_littlefs.h>
#include <nvs.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using namespace frestore;
FreStoreTrust trust{CONFIG_FRESLEST_STORE_ORIGIN, CONFIG_FRESLEST_STORE_PUBLIC_KEY};
FreStoreTrust candidate{};
char keyFingerprint[17];
bool trustLoaded = false, useStarter = true;
constexpr char FlashRoot[] = "/littlefs/FRESTORE";
char flashDirectory[sizeof(FlashRoot) + 17];

enum class Job { Scan, Catalog, Install, Remove, Open, Pair, Import };
enum class View { List, Detail, Card, Servers, Trust };
enum class Edit { None, Search, Address };
struct Installed { char id[25], name[35]; uint32_t version; bool legacy; };
struct Selection { App offer; Installed local; };
EXT_RAM_BSS_ATTR App offers[MaxApps];
EXT_RAM_BSS_ATTR Installed installed[MaxApps];
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
int selected, category, offset, cardTop, serverRow;
unsigned shownProgress = 101;
char query[35] = "";
constexpr const char *categories[] = {"All categories", "Tools", "Education", "Media"};
constexpr const char *serverOptions[] = {
    "Starter catalog", "Saved server", "Add HTTPS server", "Import previous SD settings"
};

class LegacyAccess {
    const bool usb;
public:
    const bool ok;
    explicit LegacyAccess(bool needed = true) : usb(needed && !freslestSdMounted()), ok(!usb || usbStorageLock()) {}
    ~LegacyAccess() { if (usb && ok) usbStorageUnlock(); }
};

const char *legacyRoot() { return freslestSdMounted() ? freslestSdRoot() : usbStorageRoot(); }
void legacyDirectory(char (&out)[48]) { snprintf(out, sizeof(out), "%s/FRESTORE", legacyRoot()); }

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
    char fingerprintHex[17];
    flashDirectory[0] = 0;
    if (fingerprint(activeStore().key, fingerprintHex))
        snprintf(flashDirectory, sizeof(flashDirectory), "%s/%s", FlashRoot, fingerprintHex);
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
bool fetch(const char *origin, const char *path, size_t capacity, size_t &length) {
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
        else if (bytes <= 0 || bytes > (int64_t)capacity || esp_http_client_is_chunked_response(client))
            message = "Invalid file size from the server.";
        else {
            const int64_t deadline = esp_timer_get_time() + 25000000;
            while (!cancelled.load() && length < (size_t)bytes && esp_timer_get_time() < deadline) {
                int count = esp_http_client_read(client, download + length, std::min<size_t>(1024, bytes - length));
                if (count <= 0) break;
                length += count;
                percent.store(length * 100 / bytes);
            }
            ok = !cancelled.load() && length == (size_t)bytes &&
                 esp_http_client_is_complete_data_received(client);
            if (!ok) message = cancelled.load() ? "Download cancelled." : "Download interrupted. Try again.";
        }
    }
    esp_http_client_cleanup(client);
    return ok;
}

void path(char (&out)[80], const char *directory, const char *id, char slot) {
    snprintf(out, sizeof(out), "%s/%s.%c", directory, id, slot);
}
bool readSlot(const char *directory, const char *id, char slot, Card &out) {
    char file[80]; path(file, directory, id, slot);
    FILE *f = fopen(file, "rb");
    if (!f) return false;
    size_t n = fread(fileData, 1, sizeof(fileData), f);
    bool complete = !ferror(f) && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    return complete && parseCard(fileData, n, activeStore().key, out) && !strcmp(out.id, id);
}
// The previous verified slot stays available while the next one is written.
char newest(const char *directory, const char *id, Card &out) {
    const bool a = readSlot(directory, id, 'a', out);
    const bool b = readSlot(directory, id, 'b', parsedCard);
    if (b && (!a || parsedCard.version > out.version)) { out = parsedCard; return 'b'; }
    return a ? 'a' : '\0';
}
int localIndex(const char *id) {
    for (unsigned i = 0; i < installedCount; ++i) if (!strcmp(installed[i].id, id)) return i;
    return -1;
}
void scanDirectory(const char *directory, bool legacy) {
    if (!directory[0]) return;
    DIR *dir = opendir(directory);
    if (!dir) return;
    for (unsigned inspected = 0; inspected < 128 && installedCount < MaxApps; ++inspected) {
        const dirent *entry = readdir(dir);
        if (!entry) break;
        const size_t n = strlen(entry->d_name);
        if (n < 3 || n > 26 || entry->d_name[n-2] != '.') continue;
        const char slot = tolower(static_cast<unsigned char>(entry->d_name[n-1]));
        if (slot != 'a' && slot != 'b') continue;
        char id[25];
        for (size_t i = 0; i < n - 2; ++i) id[i] = tolower(static_cast<unsigned char>(entry->d_name[i]));
        id[n-2] = 0;
        if (!validId(id) || localIndex(id) >= 0) continue;
        vTaskDelay(1);
        if (!newest(directory, id, card)) continue;
        auto &item = installed[installedCount++];
        strcpy(item.id, card.id);
        strcpy(item.name, card.name);
        item.version = card.version;
        item.legacy = legacy;
    }
    closedir(dir);
}
void scan() {
    installedCount = 0;
    scanDirectory(flashDirectory, false);
    if (!useStarter) {
        LegacyAccess disk;
        if (disk.ok) {
            char directory[48]; legacyDirectory(directory);
            scanDirectory(directory, true);
        }
    }
}
bool removeFile(const char *file) { return unlink(file) == 0 || errno == ENOENT; }
bool makeDirectory(const char *directory) { return mkdir(directory, 0777) == 0 || errno == EEXIST; }
void install(size_t length) {
    const App &offer = target.offer;
    if (!matches(download, length, offer) || !parseCard(download, length, activeStore().key, card) ||
        strcmp(offer.id, card.id) || strcmp(offer.name, card.name) || offer.version != card.version) {
        message = "Invalid package signature, hash or ID.";
        return;
    }
    size_t total, used;
    if (!flashDirectory[0] || esp_littlefs_info("spiffs", &total, &used) != ESP_OK) {
        message = "Internal flash is unavailable.";
        return;
    }
    const char current = newest(flashDirectory, offer.id, card);
    if (current && card.version >= offer.version) { message = "This version is already installed."; return; }
    if (!current && installedCount >= MaxApps && localIndex(offer.id) < 0) {
        message = "Installed app limit reached.";
        return;
    }
    if (used > total || total - used < length + 4096) { message = "Not enough free internal flash."; return; }
    if (cancelled.load()) { message = "Installation cancelled."; return; }
    if (!makeDirectory(FlashRoot) || !makeDirectory(flashDirectory)) {
        message = "Cannot create the internal app folder.";
        return;
    }
    char pending[80], next[80];
    path(pending, flashDirectory, offer.id, 't');
    path(next, flashDirectory, offer.id, current == 'a' ? 'b' : 'a');
    FILE *f = fopen(pending, "wb");
    bool saved = false;
    if (f) {
        saved = fwrite(download, 1, length, f) == length && fflush(f) == 0 && fsync(fileno(f)) == 0;
        if (fclose(f)) saved = false;
    }
    saved = saved && !cancelled.load() && rename(pending, next) == 0;
    if (!saved) removeFile(pending);
    message = saved ? "Installed to internal flash." :
              cancelled.load() ? "Installation cancelled." : "Cannot write to internal flash. Try again.";
    scan();
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
        char route[80];
        snprintf(route, sizeof(route), "/v1/packages/%s/%lu", target.offer.id, (unsigned long)target.offer.version);
        size_t length;
        if (fetch(activeStore().origin, route, target.offer.size, length)) install(length);
    } else if (job == Job::Scan) {
        scan();
        message = installedCount ? "Installed apps loaded." : "No installed apps yet.";
    } else if (job == Job::Pair) {
        requestPublisher();
    } else if (job == Job::Import) {
        LegacyAccess disk;
        message = disk.ok && importTrust() ? "" : "No valid previous settings on SD or USB.";
    } else {
        LegacyAccess disk(target.local.legacy);
        char legacy[48]; legacyDirectory(legacy);
        const char *directory = target.local.legacy ? legacy : flashDirectory;
        if (!disk.ok) {
            message = "Reconnect the storage containing this app.";
        } else if (job == Job::Open) {
            message = newest(directory, target.local.id, card) ? "" : "Installed package is missing or invalid.";
        } else {
            char old[80], active[80], pending[80];
            const char current = newest(directory, target.local.id, card);
            path(old, directory, target.local.id, current == 'a' ? 'b' : 'a');
            path(active, directory, target.local.id, current == 'a' ? 'a' : 'b');
            path(pending, directory, target.local.id, 't');
            message = removeFile(pending) && removeFile(old) && removeFile(active) ?
                      "App removed." : "Cannot remove the app.";
        }
    }
    if (job == Job::Remove) scan();
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
bool canOpen() { return installedOnly || (!target.local.legacy && target.local.version >= target.offer.version); }
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
    if (view == View::Servers) {
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
        if (installedOnly) tft.printf("Version %lu", (unsigned long)local.version);
        else {
            tft.printf("Version %lu  %lu B  ESP32-S3 / ABI 1",
                       (unsigned long)offer.version, (unsigned long)offer.size);
            label(12, 80, offer.developer);
            wrap(12, 98, offer.description);
        }
        const char *action = canOpen() ? "Open" : (local.version && !local.legacy ? "Update" : "Install");
        button(12, h - 76, 84, action);
        if (local.version) button(108, h - 76, 84, "Remove");
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
    if (!useStarter && !freslestSdMounted()) {
        usbStorageBegin();
        diskRevision = usbStorageInfo().revision;
    }
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
    if (!useStarter && !freslestSdMounted() && view != View::Trust && view != View::Servers) {
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
    if (view == View::Detail) {
        if (target.local.version && (bFire.pressed() || usbInputHit(108, h - 76, 84, 20))) start(Job::Remove);
        else if (bOk.pressed() || usbInputHit(12, h - 76, 84, 20)) {
            start(canOpen() ? Job::Open : Job::Install);
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
