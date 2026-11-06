#include "frestore.h"
#include "frestore_package.h"
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "apps.h"
#include "desktop.h"
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
#include <esp_vfs_fat.h>
#include <nvs.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using namespace frestore;
FreStoreTrust trust{CONFIG_FRESLEST_STORE_ORIGIN, CONFIG_FRESLEST_STORE_PUBLIC_KEY};
FreStoreTrust candidate{};
char keyFingerprint[17];
bool trustLoaded = false;

enum class Job { Scan, Catalog, Install, Remove, Open, Import };
enum class View { List, Detail, Card, Trust };
struct Installed { char id[25], name[35]; uint32_t version; };
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
struct StoreText {
    const char *tr = "";
    const char *en = "";
    bool empty() const { return !en[0]; }
    const char *label() const { return desktopHasWindow() ? en : tr; }
};
constexpr StoreText TrustRequired{"Magaza adresi ve yayin anahtari gerekli.", "Set the server URL and publisher key."};
constexpr StoreText DiskRequired{"FAT SD karti USB okuyucuyla baglayin.", "Connect a FAT SD card in a USB reader."};
StoreText message;
Job job;
View view = View::List;
bool installedOnly, editing;
int selected, category, offset, cardTop;
unsigned shownProgress = 101;
char query[35] = "";
constexpr StoreText categories[] = {
    {"Tum kategoriler", "All categories"}, {"Araclar", "Tools"}, {"Egitim", "Education"}, {"Medya", "Media"}
};

class DiskAccess {
public:
    const bool ok = usbStorageLock();
    ~DiskAccess() { if (ok) usbStorageUnlock(); }
};

bool validOrigin(const char *origin) {
    if (strncmp(origin, "https://", 8) || !origin[8]) return false;
    for (const char *p = origin + 8; *p; ++p)
        if (!isalnum(static_cast<unsigned char>(*p)) && *p != '.' && *p != '-' && *p != ':') return false;
    return true;
}
bool configured() { return trust.key[0] && validOrigin(trust.origin); }
void loadTrust() {
    if (trustLoaded) return;
    trustLoaded = true;
    nvs_handle_t nvs;
    if (nvs_open("fl_store", NVS_READONLY, &nvs) != ESP_OK) return;
    size_t size = sizeof(candidate);
    if (nvs_get_blob(nvs, "trust_v1", &candidate, &size) == ESP_OK && size == sizeof(candidate) &&
        !candidate.origin[sizeof(candidate.origin) - 1] && !candidate.key[sizeof(candidate.key) - 1] &&
        validOrigin(candidate.origin) && fingerprint(candidate.key, keyFingerprint)) trust = candidate;
    nvs_close(nvs);
}
bool saveTrust() {
    nvs_handle_t nvs;
    if (nvs_open("fl_store", NVS_READWRITE, &nvs) != ESP_OK) return false;
    const bool saved = nvs_set_blob(nvs, "trust_v1", &candidate, sizeof(candidate)) == ESP_OK &&
                       nvs_commit(nvs) == ESP_OK;
    nvs_close(nvs);
    if (saved) { trust = candidate; offerCount = installedCount = 0; diskRevision = UINT32_MAX; }
    return saved;
}
bool fetch(const char *path, size_t capacity, size_t &length) {
    length = 0;
    if (!configured()) { message = TrustRequired; return false; }
    if (WiFi.status() != WL_CONNECTED) { message = {"Once Wi-Fi agina baglanin.", "Connect to Wi-Fi first."}; return false; }
    struct tm time{};
    if (!rtcGetLocal(time)) { message = {"HTTPS icin saat esitlemesini bekleyin.", "Waiting for the clock to sync for HTTPS."}; return false; }
    char url[256];
    int n = snprintf(url, sizeof(url), "%s%s", trust.origin, path);
    if (n < 0 || n >= (int)sizeof(url)) { message = {"Magaza adresi gecersiz.", "Invalid store URL."}; return false; }
    esp_http_client_config_t config{};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.disable_auto_redirect = true;
    config.timeout_ms = 4000;
    config.buffer_size = 1024;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    message = {"HTTPS baglantisi kurulamadi.", "HTTPS connection failed."};
    if (!client) return false;
    bool ok = false;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        const int64_t bytes = esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (status != 200) message = status == 404 ? StoreText{"Surum sunucuda bulunamadi.", "Version not found on the server."}
                                                   : StoreText{"Sunucu istegi basarisiz.", "Server request failed."};
        else if (bytes <= 0 || bytes > (int64_t)capacity || esp_http_client_is_chunked_response(client))
            message = {"Sunucu dosya boyutu gecersiz.", "Invalid file size from the server."};
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
            if (!ok) message = cancelled.load() ? StoreText{"Indirme iptal edildi.", "Download cancelled."}
                                                : StoreText{"Indirme yarida kesildi. Tekrar deneyin.", "Download interrupted. Try again."};
        }
    }
    esp_http_client_cleanup(client);
    return ok;
}

void path(char (&out)[80], const char *id, char slot) {
    snprintf(out, sizeof(out), "%s/FRESTORE/%s.%c", usbStorageRoot(), id, slot);
}
void directory(char (&out)[48]) { snprintf(out, sizeof(out), "%s/FRESTORE", usbStorageRoot()); }
bool readSlot(const char *id, char slot, Card &out) {
    char file[80]; path(file, id, slot);
    FILE *f = fopen(file, "rb");
    if (!f) return false;
    size_t n = fread(fileData, 1, sizeof(fileData), f);
    bool complete = !ferror(f) && fgetc(f) == EOF && !ferror(f);
    fclose(f);
    return complete && parseCard(fileData, n, trust.key, out) && !strcmp(out.id, id);
}
// The previous verified slot stays available while the next one is written.
char newest(const char *id, Card &out) {
    const bool a = readSlot(id, 'a', out);
    const bool b = readSlot(id, 'b', parsedCard);
    if (b && (!a || parsedCard.version > out.version)) { out = parsedCard; return 'b'; }
    return a ? 'a' : '\0';
}
int localIndex(const char *id) {
    for (unsigned i = 0; i < installedCount; ++i) if (!strcmp(installed[i].id, id)) return i;
    return -1;
}
void scanLocked() {
    installedCount = 0;
    if (!trust.key[0]) return;
    char root[48]; directory(root);
    DIR *dir = opendir(root);
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
        if (!newest(id, card)) continue;
        auto &item = installed[installedCount++];
        strcpy(item.id, card.id);
        strcpy(item.name, card.name);
        item.version = card.version;
    }
    closedir(dir);
}
bool removeFile(const char *file) { return unlink(file) == 0 || errno == ENOENT; }
void install(size_t length) {
    const App &offer = target.offer;
    if (!matches(download, length, offer) || !parseCard(download, length, trust.key, card) ||
        strcmp(offer.id, card.id) || strcmp(offer.name, card.name) || offer.version != card.version) {
        message = {"Paket imzasi, ozeti veya kimligi gecersiz.", "Invalid package signature, hash or ID."};
        return;
    }
    DiskAccess disk;
    if (!disk.ok) { message = DiskRequired; return; }
    char root[48]; directory(root);
    if (mkdir(root, 0777) && errno != EEXIST) { message = {"SD kart dizini acilamadi.", "Cannot create the SD card folder."}; return; }
    const char current = newest(offer.id, card);
    if (current && card.version >= offer.version) { message = {"Bu surum zaten yuklu.", "This version is already installed."}; return; }
    if (!current && installedCount >= MaxApps) { message = {"Yuklu uygulama sinirina ulasildi.", "Installed app limit reached."}; return; }
    uint64_t total, available;
    if (esp_vfs_fat_info(usbStorageRoot(), &total, &available) != ESP_OK || available < length + 4096) {
        message = {"SD kartta yeterli bos alan yok.", "Not enough free space on the SD card."};
        return;
    }
    if (cancelled.load()) { message = {"Kurulum iptal edildi.", "Installation cancelled."}; return; }
    char pending[80], next[80];
    path(pending, offer.id, 't');
    path(next, offer.id, current == 'a' ? 'b' : 'a');
    FILE *f = fopen(pending, "wb");
    bool saved = false;
    if (f) {
        saved = fwrite(download, 1, length, f) == length && fflush(f) == 0 && fsync(fileno(f)) == 0;
        if (fclose(f)) saved = false;
    }
    saved = saved && usbStorageInfo().state == UsbStorageState::Ready &&
            removeFile(next) && rename(pending, next) == 0;
    if (!saved) removeFile(pending);
    message = saved ? StoreText{"Uygulama yuklendi.", "App installed."}
                    : StoreText{"SD karta yazilamadi. Tekrar deneyin.", "Cannot write to the SD card. Try again."};
    scanLocked();
}
bool importTrust() {
    char filename[48];
    snprintf(filename, sizeof(filename), "%s/FRESTORE.CFG", usbStorageRoot());
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
void worker(void *) {
    if (job == Job::Catalog) {
        size_t length;
        if (fetch("/v1/catalog", CatalogBytes, length))
            message = parseCatalog(download, length, trust.key, offers, offerCount) ?
                      StoreText{"Katalog dogrulandi.", "Catalog verified."}
                    : StoreText{"Katalog imzasi veya bicimi gecersiz.", "Invalid catalog signature or format."};
    } else if (job == Job::Install) {
        char route[80];
        snprintf(route, sizeof(route), "/v1/packages/%s/%lu", target.offer.id, (unsigned long)target.offer.version);
        size_t length;
        if (fetch(route, target.offer.size, length)) install(length);
    } else {
        DiskAccess disk;
        if (!disk.ok) {
            if (job == Job::Scan) installedCount = 0;
            message = DiskRequired;
        } else if (job == Job::Scan) {
            scanLocked();
            message = installedCount ? StoreText{"Yuklu uygulamalar okundu.", "Installed apps loaded."}
                                     : StoreText{"SD kart hazir.", "SD card ready."};
        } else if (job == Job::Open) {
            message = newest(target.local.id, card) ? StoreText{} :
                      StoreText{"Yuklu paket imzasi gecersiz.", "Installed package signature is invalid."};
        } else if (job == Job::Import) {
            message = importTrust() ? StoreText{} :
                      StoreText{"SD kartta gecerli FRESTORE.CFG bulunamadi.", "No valid FRESTORE.CFG on the SD card."};
        } else {
            char old[80], active[80], pending[80];
            const char current = newest(target.local.id, card);
            path(old, target.local.id, current == 'a' ? 'b' : 'a');
            path(active, target.local.id, current == 'a' ? 'a' : 'b');
            path(pending, target.local.id, 't');
            message = removeFile(pending) && removeFile(old) && removeFile(active) ?
                      StoreText{"Uygulama kaldirildi.", "App removed."}
                    : StoreText{"SD karttan silinemedi.", "Cannot remove app from the SD card."};
            scanLocked();
        }
    }
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
        message = {"FreStore icin yeterli bellek yok.", "Not enough memory for FreStore."};
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
bool cancellable() { return job == Job::Catalog || job == Job::Install; }
void draw() {
    const int w = tft.width(), h = tft.height();
    tft.fillScreen(TH().bg);
    const bool desktop = desktopHasWindow();
    drawHeader(installedOnly ? (desktop ? "Installed Apps" : "Yuklu Uygulamalar") : "FreStore");
    drawFooter(desktop ? "OK Select  FIRE Refresh" : "OK Sec  FIRE Yenile",
               desktop ? "BACK Close / Up" : "BACK Geri");
    tft.setTextSize(1);
    tft.setTextColor(TH().text);
    if (working) {
        const unsigned progress = percent.load();
        shownProgress = progress;
        label(12, 80, job == Job::Scan ? (desktop ? "Reading SD card..." : "SD kart okunuyor...")
                                       : (desktop ? "Working..." : "FreStore islem yapiyor..."));
        if (cancellable()) {
            tft.drawRect(12, 100, w - 24, 14, TH().dim);
            tft.fillRect(14, 102, (w - 28) * progress / 100, 10, TH().accent);
            tft.setCursor(12, 124); tft.printf("%u%%", progress);
            button(12, 146, 84, desktop ? "Cancel" : "Iptal");
        }
        return;
    }
    if (view == View::Card) {
        label(12, 48, card.name);
        for (int i = 0; i < (h - 91) / 13 && i + cardTop < (int)card.count; ++i)
            label(12, 68 + i * 13, card.lines[i + cardTop]);
        return;
    }
    if (view == View::Trust) {
        label(12, 48, desktop ? "Trust this server and publisher key?" : "Bu sunucuya ve yayin anahtarina guvenilsin mi?");
        wrap(12, 67, candidate.origin);
        tft.setCursor(12, 117); tft.printf(desktop ? "SHA-256 / first 16: %s" : "SHA-256 / ilk 16: %s", keyFingerprint);
        button(12, h - 76, 90, desktop ? "Confirm" : "Onayla");
    } else if (view == View::Detail) {
        const App &offer = target.offer;
        const auto &local = target.local;
        label(12, 48, installedOnly ? local.name : offer.name);
        tft.setCursor(12, 64);
        if (installedOnly) tft.printf(desktop ? "Version %lu" : "Surum %lu", (unsigned long)local.version);
        else {
            tft.printf(desktop ? "Version %lu  %lu B  ESP32-S3 / ABI 1" : "Surum %lu  %lu B  ESP32-S3 / ABI 1",
                       (unsigned long)offer.version, (unsigned long)offer.size);
            label(12, 80, offer.developer);
            wrap(12, 98, offer.description);
        }
        const StoreText action = installedOnly || local.version >= offer.version ? StoreText{"Ac", "Open"} :
                                 local.version ? StoreText{"Guncelle", "Update"} : StoreText{"Yukle", "Install"};
        button(12, h - 76, 84, action.label());
        if (local.version) button(108, h - 76, 84, desktop ? "Remove" : "Kaldir");
    } else {
        button(12, 40, 52, desktop ? "Search" : "Ara");
        button(70, 40, 60, desktop ? "Refresh" : "Yenile");
        if (!installedOnly) {
            button(136, 40, 101, categories[category].label());
            button(243, 40, 65, desktop ? "Server" : "Sunucu");
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
    if (!message.empty()) {
        tft.setCursor(12, h - 43);
        tft.printf("%.*s", (w - 24) / 6, message.label());
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

void frestoreEnter(bool local) {
    loadTrust();
    installedOnly = local;
    view = View::List;
    selected = offset = category = 0;
    query[0] = 0;
    editing = false;
    usbStorageBegin();
    if (!working && !local && !offerCount) {
        if (configured()) start(Job::Catalog);
        else message = TrustRequired;
    }
    draw();
}
void frestoreExit() {
    if (editing) textEntryCancel();
    editing = false;
}
void frestoreLoop() {
    if (working) {
        if (!completed.load(std::memory_order_acquire)) {
            if (cancellable() && (bBack.pressed() || usbInputHit(12, 146, 84, 20))) cancelled.store(true);
            if (shownProgress != percent.load()) draw();
            return;
        }
        working = false;
        if (job == Job::Open && view == View::Detail && message.empty()) { view = View::Card; cardTop = 0; }
        if (job == Job::Import && message.empty() && !installedOnly) view = View::Trust;
        if (job == Job::Install || job == Job::Remove || job == Job::Catalog) {
            view = View::List;
            selected = offset = 0;
        }
        draw();
        return;
    }
    const auto disk = usbStorageInfo();
    if (disk.revision != diskRevision && view != View::Trust) {
        diskRevision = disk.revision;
        if (disk.state == UsbStorageState::Ready) {
            start(Job::Scan);
            view = View::List;
            draw();
            return;
        }
        installedCount = 0;
        if (installedOnly || view == View::Card) view = View::List;
        target.local = {};
        draw();
    }
    if (editing) {
        const auto result = textEntryTakeResult(query, sizeof(query));
        if (result == TextEntryResult::None) return;
        editing = false;
        selected = offset = 0;
        draw();
        return;
    }
    if (bBack.pressed()) {
        if (view == View::Card) view = View::Detail;
        else if (view == View::Detail || view == View::Trust) view = View::List;
        else { enterApp(APP_APPS); return; }
        draw();
        return;
    }
    const int step = bDown.pressed() || usbPointer.wheel < 0 ? 1 :
                     bUp.pressed() || usbPointer.wheel > 0 ? -1 : 0;
    const int h = tft.height();
    if (view == View::Trust) {
        if (bOk.pressed() || usbInputHit(12, h - 76, 90, 20)) {
            message = saveTrust() ? StoreText{"Sunucu kaydedildi. Katalogu yenileyin.", "Server saved. Refresh the catalog."}
                                  : StoreText{"Sunucu ayarlari kaydedilemedi.", "Cannot save server settings."};
            view = View::List;
            draw();
        }
        return;
    }
    if (view == View::Card) {
        if (step) { cardTop = constrain(cardTop + step, 0, std::max(0, (int)card.count - (h - 91) / 13)); draw(); }
        return;
    }
    if (view == View::Detail) {
        if (target.local.version && (bFire.pressed() || usbInputHit(108, h - 76, 84, 20))) start(Job::Remove);
        else if (bOk.pressed() || usbInputHit(12, h - 76, 84, 20)) {
            const bool open = installedOnly || target.local.version >= target.offer.version;
            start(open ? Job::Open : Job::Install);
        } else return;
        draw();
        return;
    }
    if (usbInputHit(12, 40, 52, 20) || (bFire.pressed() && bRight.down())) {
        textEntryBegin(desktopHasWindow() ? "Search FreStore" : "FreStore Ara", query, sizeof(query) - 1);
        editing = true;
        return;
    }
    if (!installedOnly && (usbInputHit(243, 40, 65, 20) ||
        (bFire.pressed() && (!configured() || bLeft.down())))) {
        start(Job::Import); draw(); return;
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
