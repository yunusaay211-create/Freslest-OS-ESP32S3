#include "network_update.h"
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "apps.h"
#include "audio_manager.h"
#include "desktop.h"
#include "frestore.h"
#include "frestore_package.h"
#include "freslest_update.h"
#include "system_status.h"
#include <WiFi.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <esp_app_desc.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <freslest_ota.h>

namespace {
#if defined(FRESLEST_UPDATE_VGA_PROFILE)
constexpr const char *Profile = "vga";
#elif FRESLEST_LCD_MODEL == 9341
constexpr const char *Profile = "ili9341";
#else
constexpr const char *Profile = "ili9488";
#endif

enum class Page { Ready, Checking, Available, Installing, Error };
enum class Transfer { Download, Cancelled, Flash };
Page page = Page::Ready;
FreStoreTrust trust;
frestore::OsUpdate offer;
char fingerprint[17], error[96];
std::atomic<Transfer> transfer{Transfer::Download};
std::atomic<unsigned> percent{0};
std::atomic<bool> finished{false};
bool succeeded = false;
unsigned drawnPercent = 0;
Transfer drawnTransfer = Transfer::Download;

bool fail(const char *message) {
    snprintf(error, sizeof(error), "%s", message);
    return false;
}

bool download(const char *suffix, uint8_t *data, size_t capacity, size_t &received, bool firmware) {
    received = 0;
    char url[240];
    const int length = snprintf(url, sizeof(url), "%s/v1/os/%s/%s", trust.origin, Profile, suffix);
    if (length < 0 || length >= (int)sizeof(url)) return fail("Server address is too long.");
    esp_http_client_config_t config{};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.disable_auto_redirect = true;
    config.timeout_ms = 6000;
    config.buffer_size = 2048;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return fail("Cannot allocate the HTTPS connection.");
    bool ok = false;
    fail("HTTPS connection failed. Check Wi-Fi, DNS and the clock.");
    if (esp_http_client_open(client, 0) == ESP_OK) {
        const int64_t bytes = esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (status == 404) fail("No update published for this profile.");
        else if (status != 200) fail("Update server returned an error.");
        else if (bytes <= 0 || bytes > (int64_t)capacity ||
                 (firmware && bytes != (int64_t)capacity) || esp_http_client_is_chunked_response(client))
            fail("Invalid update response size.");
        else {
            const size_t total = bytes;
            const int64_t deadline = esp_timer_get_time() + (firmware ? 75000000 : 15000000);
            while (received < total && transfer.load() != Transfer::Cancelled && esp_timer_get_time() < deadline) {
                const int count = esp_http_client_read(client, reinterpret_cast<char *>(data + received),
                                                       std::min<size_t>(4096, total - received));
                if (count <= 0) break;
                received += count;
                percent.store((received * (firmware ? 25 : 100)) / total);
            }
            ok = received == total && esp_http_client_is_complete_data_received(client);
            if (!ok) fail("Update download interrupted. Try again.");
        }
    }
    esp_http_client_cleanup(client);
    if (transfer.load() == Transfer::Cancelled) return fail("Download cancelled.");
    return ok;
}

bool check() {
    char manifest[frestore::OsManifestBytes];
    size_t length;
    if (!download("manifest", reinterpret_cast<uint8_t *>(manifest), sizeof(manifest), length, false))
        return false;
    if (!frestore::parseOsUpdate(manifest, length, trust.key, Profile, offer))
        return fail("Update signature or compatibility check failed.");
    if (!frestore::newerOsVersion(offer.version, esp_app_get_description()->version))
        return fail("No newer signed version available.");
    return true;
}

bool install() {
    auto *image = static_cast<uint8_t *>(heap_caps_malloc(offer.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!image) return fail("Not enough PSRAM to verify this update.");
    char hash[65];
    constexpr char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < sizeof(offer.digest); ++i) {
        hash[i * 2] = digits[offer.digest[i] >> 4];
        hash[i * 2 + 1] = digits[offer.digest[i] & 15];
    }
    hash[64] = 0;
    size_t length;
    bool ok = download(hash, image, offer.size, length, true);
    if (ok) {
        Transfer expected = Transfer::Download;
        if (!transfer.compare_exchange_strong(expected, Transfer::Flash)) ok = fail("Download cancelled.");
    }
    if (ok) {
        freslest_update_package package{};
        package.target = FRESLEST_UPDATE_OS;
        package.image_size = offer.size;
        strcpy(package.version, offer.version);
        memcpy(package.sha256, offer.digest, sizeof(package.sha256));
        ok = freslest_update_stage_os(image, length, &package,
            [](unsigned value, const char *) { percent.store(value); }, error, sizeof(error));
    }
    heap_caps_free(image);
    return ok;
}

void worker(void *installing) {
    succeeded = installing ? install() : check();
    finished.store(true, std::memory_order_release);
    vTaskDelete(nullptr);
}

void text(int y, const char *message) {
    const int columns = (tft.width() - 40) / 6;
    for (int offset = 0, length = strlen(message); offset < length; offset += columns) {
        tft.setCursor(20, y);
        tft.printf("%.*s", columns, message + offset);
        y += 12;
    }
}

bool active() { return page == Page::Checking || page == Page::Installing; }

void draw() {
    tft.fillScreen(TH().bg);
    drawHeader("Wi-Fi Update");
    drawFooter(active() ? "Network update in progress" : "OK Select", "BACK Close / Cancel");
    tft.setTextSize(1);
    tft.setTextColor(TH().text, TH().bg);
    tft.setCursor(20, 54);
    tft.printf("Installed OS: %s  |  Profile: %s", esp_app_get_description()->version, Profile);
    if (active()) {
        drawnPercent = percent.load();
        drawnTransfer = transfer.load();
        text(94, page == Page::Checking ? "Checking for signed updates..." :
                 drawnTransfer == Transfer::Flash ? "Verifying and installing. Keep power connected." :
                 drawnTransfer == Transfer::Cancelled ? "Cancelling download..." : "Downloading verified release...");
        const int width = tft.width() - 40;
        tft.drawRect(20, 124, width, 16, TH().dim);
        tft.fillRect(22, 126, (width - 4) * drawnPercent / 100, 12, TH().accent);
        tft.setCursor(20, 150); tft.printf("%u%%", drawnPercent);
        return;
    }
    if (page == Page::Available) {
        tft.setCursor(20, 88); tft.printf("Freslest OS %s is available.", offer.version);
        tft.setCursor(20, 106); tft.printf("Publisher SHA-256: %s", fingerprint);
        text(122, trust.origin);
    } else if (page == Page::Error) text(88, error);
    else text(88, "Check your trusted FreStore server for a signed OS release.");

    const int y = tft.height() - 60;
    tft.fillRoundRect(20, y, 154, 27, 4, TH().selbg);
    tft.setTextColor(TH().text, TH().selbg);
    tft.setCursor(30, y + 9);
    tft.print(page == Page::Available ? "Install and restart" : "Check for updates");
}

void start(bool installing) {
    if (!frestoreReadTrust(trust) || !frestore::fingerprint(trust.key, fingerprint)) {
        fail("Set a trusted server and publisher key in FreStore first.");
        page = Page::Error;
    } else if (WiFi.status() != WL_CONNECTED) {
        fail("Connect to Wi-Fi first.");
        page = Page::Error;
    } else if (struct tm clock{}; !rtcGetLocal(clock)) {
        fail("Wait for the clock to sync for HTTPS.");
        page = Page::Error;
    } else if (!freslest_ota_inactive_os()) {
        fail("The OS must complete its startup check before updating.");
        page = Page::Error;
    } else {
        page = installing ? Page::Installing : Page::Checking;
        percent.store(0);
        transfer.store(Transfer::Download);
        finished.store(false);
        if (xTaskCreatePinnedToCore(worker, "network_ota", 14336, installing ? &offer : nullptr,
                                   2, nullptr, 0) != pdPASS) {
            fail("Not enough RAM to start the update task.");
            page = Page::Error;
        }
    }
    draw();
}
} // namespace

void networkUpdateEnter() {
    page = Page::Ready;
    draw();
}

void networkUpdateLoop() {
    if (active()) {
        if (!finished.load(std::memory_order_acquire)) {
            if (bBack.pressed()) {
                Transfer expected = Transfer::Download;
                transfer.compare_exchange_strong(expected, Transfer::Cancelled);
            }
            if (drawnPercent != percent.load() || drawnTransfer != transfer.load()) draw();
            return;
        }
        if (succeeded && page == Page::Installing) {
            text(170, "Verified. Restarting to test the new OS.");
            settingsSave();
            audioStop();
            gfxPresent();
            delay(350);
            ESP.restart();
            return;
        }
        page = succeeded ? Page::Available : Page::Error;
        draw();
        return;
    }
    if (bBack.pressed()) {
        if (!desktopCloseModal()) enterApp(APP_APPS);
    } else if (bOk.pressed() || usbInputHit(20, tft.height() - 60, 154, 27)) {
        start(page == Page::Available);
    }
}
