// SPDX-License-Identifier: MIT
// IDF owns the two-sector boot journal. NVS stores intent and previous OS.
#include "freslest_ota.h"
#include <stdatomic.h>
#include <string.h>
#ifndef FRESLEST_OTA_HOST_TEST
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#endif
enum { IDLE, REQUEST, WRITING, TRIAL };
typedef struct {
  uint32_t magic;
  uint8_t schema, mode, source, target, active, previous, reserved[6];
} journal;
static journal state;
static bool begun, recovered;
static atomic_bool pending, ready;
static unsigned frames;
static int64_t started;
static esp_timer_handle_t deadline;
static const esp_partition_t *slot(unsigned id) {
  return id > 1 ? NULL
                : esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                           ESP_PARTITION_SUBTYPE_APP_OTA_0 + id,
                                           id ? "os_b" : "os_a");
}
static unsigned slot_id(const esp_partition_t *p) {
  if (p && p->type == ESP_PARTITION_TYPE_APP &&
      p->size == FRESLEST_OS_SLOT_SIZE) {
    if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 && p->address == 0x100000)
      return 0;
    if (p->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1 && p->address == 0x2b0000)
      return 1;
  }
  return 255;
}
bool freslest_ota_layout_valid(void) {
  const esp_partition_t *d = esp_partition_find_first(
                            0, ESP_PARTITION_SUBTYPE_APP_OTA_2, "doom"),
                        *w = esp_partition_find_first(0x42, 6, "wad");
  const esp_partition_t *fs = esp_partition_find_first(
                            1, ESP_PARTITION_SUBTYPE_ANY, "spiffs"),
                        *sv = esp_partition_find_first(1, 0x40, "saves");
  return slot_id(slot(0)) == 0 && slot_id(slot(1)) == 1 && d &&
         d->address == 0x20000 && d->size == 0xe0000 && w &&
         w->address == 0x460000 && w->size == 0x950000 && fs &&
         fs->address == 0xdb0000 && fs->size == 0x50000 && sv &&
         sv->address == 0xe00000 && sv->size == 0x200000;
}
static esp_err_t save(const journal *next) {
  nvs_handle_t h;
  esp_err_t e = nvs_open("fl_update", NVS_READWRITE, &h);
  if (e != ESP_OK)
    return e;
  e = nvs_set_blob(h, "journal", next, sizeof(*next));
  if (e == ESP_OK)
    e = nvs_commit(h);
  nvs_close(h);
  if (e == ESP_OK)
    state = *next;
  return e;
}
static esp_err_t load(void) {
  nvs_handle_t h;
  size_t n = sizeof(state);
  esp_err_t e = nvs_open("fl_update", NVS_READWRITE, &h);
  if (e != ESP_OK)
    return e;
  e = nvs_get_blob(h, "journal", &state, &n);
  nvs_close(h);
  if (e == ESP_ERR_NVS_NOT_FOUND) {
    journal initial = {.magic = 0x46524f54,
                       .schema = 2,
                       .source = 255,
                       .target = 255,
                       .active = 0,
                       .previous = 255};
    unsigned current = slot_id(esp_ota_get_running_partition());
    if (current < 2)
      initial.active = current;
    return save(&initial);
  }
  if (e != ESP_OK)
    return e;
  if (n != sizeof(state) || state.magic != 0x46524f54 || state.schema != 2 ||
      state.mode > TRIAL || state.active > 1 ||
      (state.previous > 1 && state.previous != 255) ||
      ((state.mode == WRITING || state.mode == TRIAL) &&
       (state.source > 1 || state.target > 1 || state.source == state.target)))
    return ESP_ERR_INVALID_STATE;
  return ESP_OK;
}
static void reject(void *unused) {
  (void)unused;
  if (!atomic_exchange(&pending, false))
    return;
  ESP_LOGE("FreslestOTA",
           "Startup failed or timed out. Restoring previous version.");
  esp_ota_mark_app_invalid_rollback_and_reboot();
  esp_restart();
}
static esp_err_t confirm_os(unsigned current) {
  journal next = state;
  if (next.mode == TRIAL && next.target == current) {
    next.previous = next.source;
    next.source = next.target = 255;
    next.mode = IDLE;
  }
  next.active = current;
  return save(&next);
}
bool freslest_ota_boot_begin(void) {
  if (begun)
    return ready || pending;
  begun = true;
  started = esp_timer_get_time();
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t image_state;
  pending = running &&
            esp_ota_get_state_partition(running, &image_state) == ESP_OK &&
            image_state == ESP_OTA_IMG_PENDING_VERIFY;
  if (pending) {
    esp_timer_create_args_t args = {.callback = reject, .name = "ota_trial"};
    if (esp_timer_create(&args, &deadline) != ESP_OK ||
        esp_timer_start_once(deadline, 45000000) != ESP_OK) {
      reject(NULL);
      return false;
    }
  }
  if (!freslest_ota_layout_valid() || nvs_flash_init() != ESP_OK ||
      load() != ESP_OK) {
    if (pending)
      reject(NULL);
    return false;
  }
  unsigned current = slot_id(running);
  if (current < 2) {
    if ((state.mode == WRITING || state.mode == TRIAL) &&
        current == state.source) {
      journal next = state;
      next.mode = IDLE;
      next.active = current;
      next.source = next.target = next.previous = 255;
      if (save(&next) != ESP_OK) {
        if (pending)
          reject(NULL);
        return false;
      }
      recovered = true;
      ESP_LOGW("FreslestOTA",
               "Interrupted update or failed first boot; previous OS retained");
    } else if (!pending && confirm_os(current) != ESP_OK)
      return false;
  }
  ready = !pending;
  return true;
}
bool freslest_ota_health_tick(bool healthy) {
  if (!begun || (!ready && !pending))
    return false;
  if (!healthy) {
    if (pending)
      reject(NULL);
    return false;
  }
  if (!pending)
    return ready;
  if (frames < 60)
    ++frames;
  if (frames < 60 || esp_timer_get_time() - started < 10000000)
    return false;
  if (!atomic_exchange(&pending, false))
    return false;
  if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
    esp_restart();
    return false;
  }
  if (deadline) {
    esp_timer_stop(deadline);
    esp_timer_delete(deadline);
    deadline = NULL;
  }
  unsigned current = slot_id(esp_ota_get_running_partition());
  ready = current > 1 || confirm_os(current) == ESP_OK;
  ESP_LOGI("FreslestOTA", "Startup confirmed");
  return ready;
}
bool freslest_ota_ready(void) { return ready; }
bool freslest_ota_recovered(void) { return recovered; }
bool freslest_ota_take_update_request(void) {
  if (!ready || state.mode != REQUEST)
    return false;
  journal next = state;
  next.mode = IDLE;
  return save(&next) == ESP_OK;
}
esp_err_t freslest_ota_request_update(void) {
  if (!ready || state.mode != IDLE ||
      slot_id(esp_ota_get_running_partition()) > 1)
    return ESP_ERR_INVALID_STATE;
  journal next = state;
  next.mode = REQUEST;
  return save(&next);
}
const esp_partition_t *freslest_ota_active_os(void) {
  return begun ? slot(state.active) : NULL;
}
const esp_partition_t *freslest_ota_inactive_os(void) {
  unsigned current = slot_id(esp_ota_get_running_partition());
  return ready && current < 2 && freslest_ota_layout_valid() ? slot(current ^ 1)
                                                             : NULL;
}
const esp_partition_t *freslest_ota_previous_os(void) {
  if (!ready || state.previous > 1 ||
      state.previous == slot_id(esp_ota_get_running_partition()))
    return NULL;
  const esp_partition_t *p = slot(state.previous);
  esp_app_desc_t desc;
  uint8_t hash[32];
  return p && esp_ota_get_partition_description(p, &desc) == ESP_OK &&
                 !strcmp(desc.project_name, "freslest_os_s3") &&
                 esp_partition_get_sha256(p, hash) == ESP_OK
             ? p
             : NULL;
}
esp_err_t freslest_ota_begin_update(const esp_partition_t *target) {
  const esp_partition_t *allowed = freslest_ota_inactive_os();
  if (!allowed || !target || allowed->address != target->address ||
      state.mode != IDLE)
    return ESP_ERR_INVALID_STATE;
  journal next = state;
  next.mode = WRITING;
  next.source = slot_id(esp_ota_get_running_partition());
  next.target = slot_id(target);
  next.previous = 255;
  return save(&next);
}
esp_err_t freslest_ota_select_update(const esp_partition_t *target) {
  if (!ready || !target || state.mode != WRITING ||
      slot_id(target) != state.target ||
      slot_id(esp_ota_get_running_partition()) != state.source)
    return ESP_ERR_INVALID_STATE;
  journal next = state;
  next.mode = TRIAL;
  esp_err_t e = save(&next);
  return e == ESP_OK ? esp_ota_set_boot_partition(target) : e;
}
void freslest_ota_cancel_update(void) {
  if (ready) {
    journal next = state;
    next.mode = IDLE;
    next.source = next.target = 255;
    save(&next);
  }
}
esp_err_t freslest_ota_restore_previous(void) {
  const esp_partition_t *p = freslest_ota_previous_os();
  if (!p || state.mode != IDLE)
    return ESP_ERR_NOT_FOUND;
  journal before = state, next = state;
  next.mode = TRIAL;
  next.source = slot_id(esp_ota_get_running_partition());
  next.target = slot_id(p);
  esp_err_t e = save(&next);
  if (e == ESP_OK) {
    e = esp_ota_set_boot_partition(p);
    if (e != ESP_OK)
      save(&before);
  }
  return e;
}
