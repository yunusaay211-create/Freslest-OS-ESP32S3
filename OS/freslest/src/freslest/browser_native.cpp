#include "browser_native.h"
#ifdef FRESLEST_NATIVE_HTTP_TEST
#include "native_http_platform.h"
#else
#include <Arduino.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <strings.h>
namespace {
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
struct Buffer {
  NativePage *page = nullptr;
  bool busy = false;
} buffers[2];
TaskHandle_t task = nullptr;
bool active = false, requested = false;
uint32_t generation = 0;
char target[web_lite::URL]{};
int pending = -1;
NativeBrowserStatus state{false, 0, "Enter a web address. No PC is needed."};
bool current(uint32_t g) {
  portENTER_CRITICAL(&lock);
  bool yes = active && g == generation;
  portEXIT_CRITICAL(&lock);
  return yes;
}
void status(uint32_t g, bool busy, const char *s) {
  portENTER_CRITICAL(&lock);
  if (g == generation) {
    state.busy = busy;
    strlcpy(state.message, s, sizeof(state.message));
    ++state.revision;
  }
  portEXIT_CRITICAL(&lock);
}
int reserve() {
  portENTER_CRITICAL(&lock);
  int slot = -1;
  for (int i = 0; i < 2; i++)
    if (!buffers[i].busy && i != pending) {
      buffers[i].busy = true;
      slot = i;
      break;
    }
  portEXIT_CRITICAL(&lock);
  return slot;
}
struct Headers {
  char type[96]{}, location[web_lite::URL]{}, encoding[32]{};
};
esp_err_t header(esp_http_client_event_t *event) {
  if (event->event_id != HTTP_EVENT_ON_HEADER)
    return ESP_OK;
  auto *h = (Headers *)event->user_data;
  if (!strcasecmp(event->header_key, "Content-Type"))
    strlcpy(h->type, event->header_value, sizeof(h->type));
  if (!strcasecmp(event->header_key, "Location"))
    strlcpy(h->location, event->header_value, sizeof(h->location));
  if (!strcasecmp(event->header_key, "Content-Encoding"))
    strlcpy(h->encoding, event->header_value, sizeof(h->encoding));
  return ESP_OK;
}
void fail(NativePage &page, const char *s) {
  strlcpy(page.message, s, sizeof(page.message));
}
void fetch(NativePage &page, const char *url, uint32_t gen) {
  page.jpegLength = 0;
  page.image = false;
  page.httpStatus = 0;
  page.message[0] = 0;
  page.document.reset(url);
  if (WiFi.status() != WL_CONNECTED) {
    fail(page, "Connect Wi-Fi in Settings first.");
    return;
  }
  char next[web_lite::URL];
  strlcpy(next, url, sizeof(next));
  for (unsigned redirect = 0; redirect <= 5 && current(gen); redirect++) {
    if (!strncmp(next, "https://", 8) && time(nullptr) < 1704067200LL) {
      fail(page, "HTTPS needs a valid clock. Wait for Wi-Fi/NTP or set Clock, "
                 "then Reload.");
      return;
    }
    Headers headers;
    esp_http_client_config_t config{};
    config.url = next;
    config.timeout_ms = 4000;
    config.disable_auto_redirect = true;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.event_handler = header;
    config.user_data = &headers;
    config.buffer_size = 2048;
    config.buffer_size_tx = 1024;
    auto client = esp_http_client_init(&config);
    if (!client) {
      fail(page, "Not enough memory for HTTP/TLS.");
      return;
    }
    esp_http_client_set_header(client, "User-Agent",
                               "Freslest/6.12 (ESP32-S3; text browser)");
    esp_http_client_set_header(
        client, "Accept", "text/html,text/plain,image/jpeg;q=0.9,*/*;q=0.1");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    esp_err_t opened = esp_http_client_open(client, 0);
    int64_t size =
        opened == ESP_OK ? esp_http_client_fetch_headers(client) : -1;
    if (opened != ESP_OK || size < 0) {
      esp_http_client_cleanup(client);
      fail(page, "Connection failed. Check address, Wi-Fi, time and server "
                 "certificate.");
      return;
    }
    int code = esp_http_client_get_status_code(client);
    page.httpStatus = code;
    if (code == 301 || code == 302 || code == 303 || code == 307 ||
        code == 308) {
      char resolved[web_lite::URL];
      bool good = headers.location[0] &&
                  web_lite::resolveUrl(next, headers.location, resolved);
      esp_http_client_cleanup(client);
      if (!good || redirect == 5) {
        fail(page, "Redirect is invalid or exceeds the five-hop limit.");
        return;
      }
      strlcpy(next, resolved, sizeof(next));
      continue;
    }
    if (headers.encoding[0] && strcasecmp(headers.encoding, "identity")) {
      esp_http_client_cleanup(client);
      fail(page, "This server requires compressed content that this browser "
                 "cannot decode.");
      return;
    }
    web_lite::Parser parser(page.document, next);
    bool jpeg = !strncasecmp(headers.type, "image/jpeg", 10),
         plain = !strncasecmp(headers.type, "text/plain", 10),
         html = !headers.type[0] ||
                !strncasecmp(headers.type, "text/html", 9) ||
                !strncasecmp(headers.type, "application/xhtml+xml", 21);
    if (!jpeg && !plain && !html) {
      esp_http_client_cleanup(client);
      fail(page, "Unsupported content. This browser reads HTML/text and JPEG; "
                 "no JS or video codec.");
      return;
    }
    page.image = jpeg;
    size_t received = 0, limit = jpeg ? NATIVE_JPEG_MAX : 262144;
    char chunk[1024];
    uint32_t deadline = millis() + 25000;
    bool complete = false, broken = false;
    while (current(gen) && (int32_t)(millis() - deadline) < 0) {
      int count = esp_http_client_read(client, chunk, sizeof(chunk));
      if (count < 0) {
        broken = true;
        break;
      }
      if (!count) {
        complete = esp_http_client_is_complete_data_received(client);
        if (complete)
          break;
        vTaskDelay(pdMS_TO_TICKS(5));
        continue;
      }
      if ((size_t)count > limit - received) {
        page.document.truncated = true;
        broken = jpeg;
        break;
      }
      if (jpeg) {
        memcpy(page.jpeg + received, chunk, count);
        page.jpegLength += count;
      } else if (plain)
        parser.plain(chunk, count);
      else
        parser.feed(chunk, count);
      received += count;
      vTaskDelay(1);
      if (received == limit) {
        complete = esp_http_client_is_complete_data_received(client);
        if (!complete)
          page.document.truncated = true;
        break;
      }
    }
    esp_http_client_cleanup(client);
    parser.finish();
    if (!current(gen))
      return;
    if (jpeg && (!complete || broken || page.jpegLength < 4)) {
      page.jpegLength = 0;
      fail(page, "JPEG incomplete or larger than 96 KiB.");
    } else if (broken)
      fail(page, "Connection interrupted; the received text is shown.");
    else if (!complete && !page.document.truncated)
      fail(page, "Page timed out; the received text is shown.");
    else if (code >= 400)
      snprintf(page.message, sizeof(page.message),
               "HTTP %d. The server response is shown below.", code);
    else if (page.document.truncated)
      fail(page, "Page shortened to fit device limits.");
    else if (page.document.scripts)
      fail(page, "Text mode. JavaScript is not executed; some controls may be "
                 "unavailable.");
    else
      fail(page, jpeg ? "JPEG loaded directly on ESP32."
                      : "Loaded directly on ESP32. No PC/server required.");
    return;
  }
}
void worker(void *) {
  for (;;) {
    char url[web_lite::URL];
    uint32_t gen;
    bool work;
    portENTER_CRITICAL(&lock);
    gen = generation;
    work = active && requested;
    if (work)
      strlcpy(url, target, sizeof(url));
    portEXIT_CRITICAL(&lock);
    if (!work) {
      vTaskDelay(pdMS_TO_TICKS(30));
      continue;
    }
    int slot = reserve();
    if (slot < 0) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    portENTER_CRITICAL(&lock);
    if (gen == generation)
      requested = false;
    portEXIT_CRITICAL(&lock);
    fetch(*buffers[slot].page, url, gen);
    portENTER_CRITICAL(&lock);
    if (active && gen == generation) {
      if (pending >= 0)
        buffers[pending].busy = false;
      pending = slot;
      buffers[slot].busy = false;
      state.busy = false;
      strlcpy(state.message, buffers[slot].page->message,
              sizeof(state.message));
      ++state.revision;
    } else
      buffers[slot].busy = false;
    portEXIT_CRITICAL(&lock);
  }
}
} // namespace
bool nativeBrowserOpen(const char *url) {
  if (!web_lite::validUrl(url))
    return false;
  for (auto &b : buffers)
    if (!b.page) {
      b.page = (NativePage *)heap_caps_calloc(
          1, sizeof(NativePage), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!b.page)
        return false;
    }
  if (!task && xTaskCreatePinnedToCore(worker, "browser_http", 12288, nullptr,
                                       2, &task, 0) != pdPASS) {
    task = nullptr;
    return false;
  }
  portENTER_CRITICAL(&lock);
  ++generation;
  active = requested = true;
  strlcpy(target, url, sizeof(target));
  state.busy = true;
  strlcpy(state.message, "Loading directly over Wi-Fi...",
          sizeof(state.message));
  ++state.revision;
  if (pending >= 0) {
    buffers[pending].busy = false;
    pending = -1;
  }
  portEXIT_CRITICAL(&lock);
  return true;
}
void nativeBrowserCancel() {
  portENTER_CRITICAL(&lock);
  ++generation;
  active = requested = state.busy = false;
  if (pending >= 0) {
    buffers[pending].busy = false;
    pending = -1;
  }
  ++state.revision;
  portEXIT_CRITICAL(&lock);
}
NativePage *nativeBrowserTake(int &slot) {
  portENTER_CRITICAL(&lock);
  slot = pending;
  NativePage *p = nullptr;
  if (slot >= 0) {
    pending = -1;
    buffers[slot].busy = true;
    p = buffers[slot].page;
  }
  portEXIT_CRITICAL(&lock);
  return p;
}
void nativeBrowserRelease(int slot) {
  if (slot < 0 || slot >= 2)
    return;
  portENTER_CRITICAL(&lock);
  buffers[slot].busy = false;
  portEXIT_CRITICAL(&lock);
}
NativeBrowserStatus nativeBrowserStatus() {
  portENTER_CRITICAL(&lock);
  auto s = state;
  portEXIT_CRITICAL(&lock);
  return s;
}
