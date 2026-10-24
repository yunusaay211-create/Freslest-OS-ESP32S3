// BLE HID central: persistent selected peers, bounded retries, address-based
// teardown.
#include "ble_advertising.h"
#ifdef FRESLEST_BLE_HOST_TEST
#include "ble_host_platform.h"
#else
#include <Arduino.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_hidh.h>
#include <esp_heap_caps.h>
#include <freertos/queue.h>
#include <freslest_hid.h>
#include <freslest_hid_map.h>
#include <nvs.h>
#endif
#include <cstring>
namespace {
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
FreslestBleInfo info{};
struct Peer {
  esp_hidh_dev_t *dev;
  uint8_t address[6];
  uint32_t epoch;
  freslest_hid_map_t maps[2];
  uint8_t kinds[2];
};
Peer peers[2]{};
struct Saved {
  uint8_t address[6], type, kind;
  char name[38];
};
struct Store {
  uint32_t magic;
  uint16_t version, count;
  Saved peer[2];
};
struct Retry {
  bool seen, paused;
  uint8_t failures;
  uint32_t seenAt, due;
};
Store saved{0x46424c45, 1, 0, {}};
Retry retry[2]{};
struct Command {
  uint8_t op;
  FreslestBleDevice device;
};
enum { START = 1, STOP, SCAN, CONNECT, FORGET };
QueueHandle_t commands;
bool initialized = false, loaded = false, privacyReady = false,
     privateAddress = false;
bool pending = false, dirty = false, scanRequested = false;
uint8_t pairing[6]{};
uint32_t epoch = 0, pendingEpoch = 0, nextScan = 0, handshakeDeadline = 0,
         saveAfter = 0;
FreslestBleDevice pendingDevice{};
uint8_t rejects[2][6]{};
unsigned rejectCount = 0;
unsigned scanSeconds = 2;
bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }
bool same(const uint8_t *a, const uint8_t *b) { return !memcmp(a, b, 6); }
bool validAddress(const uint8_t *b) {
  unsigned zero = 0, ones = 0;
  for (int i = 0; i < 6; i++) {
    zero += b[i] == 0;
    ones += b[i] == 255;
  }
  return zero != 6 && ones != 6;
}
void status(const char *s) {
  portENTER_CRITICAL(&mux);
  strlcpy(info.status, s, sizeof(info.status));
  ++info.revision;
  portEXIT_CRITICAL(&mux);
}
bool enabled() {
  portENTER_CRITICAL(&mux);
  bool on = info.enabled;
  portEXIT_CRITICAL(&mux);
  return on;
}
int savedIndex(const uint8_t *b) {
  for (unsigned i = 0; i < saved.count; i++)
    if (same(saved.peer[i].address, b))
      return (int)i;
  return -1;
}
bool linked(const uint8_t *b) {
  for (auto &p : peers)
    if (p.dev && p.epoch == epoch && same(p.address, b))
      return true;
  return false;
}
unsigned liveCount() {
  unsigned n = 0;
  for (auto &p : peers)
    n += p.dev && p.epoch == epoch;
  return n;
}
void listDevice(FreslestBleDevice d) {
  unsigned i = 0;
  while (i < info.count && !same(info.devices[i].address, d.address))
    ++i;
  if (i == info.count) {
    if (info.count == 6)
      return;
    ++info.count;
  }
  d.connected = linked(d.address);
  d.remembered = savedIndex(d.address) >= 0;
  if (!d.name[0])
    snprintf(d.name, sizeof(d.name), "BLE HID %02X:%02X:%02X", d.address[3],
             d.address[4], d.address[5]);
  info.devices[i] = d;
  info.connected = liveCount();
  ++info.revision;
}
void refreshList() {
  info.connected = liveCount();
  for (unsigned i = 0; i < info.count; i++) {
    info.devices[i].connected = linked(info.devices[i].address);
    info.devices[i].remembered = savedIndex(info.devices[i].address) >= 0;
  }
  ++info.revision;
}
void retryFailed(int i, uint32_t now) {
  if (i < 0)
    return;
  auto &r = retry[i];
  if (r.failures < 5)
    ++r.failures;
  r.due = now + (1000u << r.failures);
  if (r.failures == 5)
    r.due = now + 30000u;
}
void rejectLater(const uint8_t *b) {
  portENTER_CRITICAL(&mux);
  if (rejectCount < 2)
    memcpy(rejects[rejectCount++], b, 6);
  portEXIT_CRITICAL(&mux);
}
bool selected(const uint8_t *b) {
  portENTER_CRITICAL(&mux);
  bool yes =
      info.enabled &&
      ((pending && pendingEpoch == epoch && same(pairing, b)) || linked(b));
  portEXIT_CRITICAL(&mux);
  return yes;
}
void loadSaved() {
  if (loaded)
    return;
  Store data{};
  nvs_handle_t n;
  size_t size = sizeof(data);
  bool ok = false;
  if (nvs_open("fl_ble", NVS_READONLY, &n) == ESP_OK) {
    ok = nvs_get_blob(n, "peers", &data, &size) == ESP_OK &&
         size == sizeof(data) && data.magic == saved.magic &&
         data.version == 1 && data.count <= 2;
    nvs_close(n);
  }
  if (ok)
    for (unsigned i = 0; i < data.count; i++)
      if (!validAddress(data.peer[i].address) || data.peer[i].type > 3 ||
          !memchr(data.peer[i].name, 0, 38))
        ok = false;
  if (!ok) {
    data = {saved.magic, 1, 0, {}};
    int count = esp_ble_get_bond_device_num();
    if (count > 0 && count <= 32) {
      auto *b = (esp_ble_bond_dev_t *)calloc((size_t)count,
                                             sizeof(esp_ble_bond_dev_t));
      if (b) {
        if (esp_ble_get_bond_device_list(&count, b) == ESP_OK)
          for (int j = 0; j < count && data.count < 2; j++) {
            auto &s = data.peer[data.count++];
            memcpy(s.address, b[j].bd_addr, 6);
            s.type = b[j].bd_addr_type;
            snprintf(s.name, sizeof(s.name), "Paired HID %02X:%02X:%02X",
                     s.address[3], s.address[4], s.address[5]);
          }
        free(b);
      }
    }
  }
  portENTER_CRITICAL(&mux);
  saved = data;
  loaded = true;
  dirty = !ok && data.count;
  saveAfter = 0;
  for (unsigned i = 0; i < saved.count; i++) {
    FreslestBleDevice d{};
    memcpy(d.address, saved.peer[i].address, 6);
    d.addressType = saved.peer[i].type;
    strlcpy(d.name, saved.peer[i].name, sizeof(d.name));
    listDevice(d);
  }
  portEXIT_CRITICAL(&mux);
}
void persist(uint32_t now) {
  Store data{};
  portENTER_CRITICAL(&mux);
  bool write = dirty && (!saveAfter || due(now, saveAfter));
  if (write) {
    data = saved;
    dirty = false;
  }
  portEXIT_CRITICAL(&mux);
  if (!write)
    return;
  nvs_handle_t n;
  esp_err_t e = nvs_open("fl_ble", NVS_READWRITE, &n);
  if (e == ESP_OK) {
    e = nvs_set_blob(n, "peers", &data, sizeof(data));
    if (e == ESP_OK)
      e = nvs_commit(n);
    nvs_close(n);
  }
  if (e != ESP_OK) {
    portENTER_CRITICAL(&mux);
    dirty = true;
    saveAfter = now + 5000;
    portEXIT_CRITICAL(&mux);
    status("Connected settings not saved. Storage retry pending.");
  }
}
void remember(const uint8_t *b, unsigned kind, const char *name) {
  int i = savedIndex(b);
  if (i < 0) {
    if (saved.count < 2)
      i = saved.count++;
    else {
      for (unsigned j = 0; j < 2; j++)
        if (!linked(saved.peer[j].address)) {
          i = (int)j;
          break;
        }
    }
  }
  if (i < 0)
    return;
  auto &s = saved.peer[i];
  memset(&s, 0, sizeof(s));
  memcpy(s.address, b, 6);
  s.type = pendingDevice.addressType;
  s.kind = (uint8_t)kind;
  strlcpy(s.name, name && name[0] ? name : pendingDevice.name, sizeof(s.name));
  for (auto &c : s.name)
    if (c && (unsigned char)c < 32)
      c = '?';
  retry[i] = {true, false, 0, millis(), 0};
  dirty = true;
  saveAfter = 0;
}
void gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p) {
  if (event == ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT) {
    portENTER_CRITICAL(&mux);
    privacyReady = true;
    privateAddress = p->local_privacy_cmpl.status == ESP_BT_STATUS_SUCCESS;
    portEXIT_CRITICAL(&mux);
  } else if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
    if (enabled() && !freslestBleInfo().busy &&
        p->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS)
      esp_ble_gap_start_scanning(scanSeconds);
    else {
      portENTER_CRITICAL(&mux);
      scanRequested = false;
      portEXIT_CRITICAL(&mux);
    }
  } else if (event == ESP_GAP_BLE_SCAN_START_COMPLETE_EVT) {
    portENTER_CRITICAL(&mux);
    scanRequested = false;
    info.scanning =
        info.enabled && p->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS;
    ++info.revision;
    portEXIT_CRITICAL(&mux);
  } else if (event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT) {
    portENTER_CRITICAL(&mux);
    scanRequested = false;
    info.scanning = false;
    ++info.revision;
    portEXIT_CRITICAL(&mux);
  } else if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
    auto &r = p->scan_rst;
    if (r.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
      portENTER_CRITICAL(&mux);
      scanRequested = false;
      info.scanning = false;
      ++info.revision;
      portEXIT_CRITICAL(&mux);
      return;
    }
    if (r.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT || !enabled())
      return;
    bool isHid = false;
    FreslestBleDevice d{};
    memcpy(d.address, r.bda, 6);
    d.addressType = r.ble_addr_type;
    size_t n = r.adv_data_len + r.scan_rsp_len;
    if (n > sizeof(r.ble_adv))
      return;
    for (size_t off = 0; off < n;) {
      unsigned len = r.ble_adv[off++];
      if (!len || len > n - off)
        break;
      unsigned type = r.ble_adv[off];
      const uint8_t *v = r.ble_adv + off + 1;
      unsigned count = len - 1;
      if (type == 2 || type == 3)
        for (unsigned j = 0; j + 1 < count; j += 2)
          if (v[j] == 0x12 && v[j + 1] == 0x18)
            isHid = true;
      if (type == 0x19 && count == 2 && v[1] == 3 && (v[0] & 0xc0) == 0xc0)
        isHid = true;
      if (type == 8 || type == 9) {
        unsigned c = count < sizeof(d.name) - 1 ? count : sizeof(d.name) - 1;
        memcpy(d.name, v, c);
        d.name[c] = 0;
        for (unsigned j = 0; j < c; j++)
          if ((unsigned char)d.name[j] < 32 || (unsigned char)d.name[j] > 126)
            d.name[j] = '?';
      }
      off += len;
    }
    uint32_t now = millis();
    portENTER_CRITICAL(&mux);
    int i = savedIndex(d.address);
    if (i >= 0) {
      retry[i].seen = true;
      retry[i].seenAt = now;
      saved.peer[i].type = d.addressType;
      if (!d.name[0])
        strlcpy(d.name, saved.peer[i].name, sizeof(d.name));
    }
    if (isHid || i >= 0)
      listDevice(d);
    portEXIT_CRITICAL(&mux);
  } else if (event == ESP_GAP_BLE_SEC_REQ_EVT)
    esp_ble_gap_security_rsp(p->ble_security.ble_req.bd_addr,
                             selected(p->ble_security.ble_req.bd_addr));
  else if (event == ESP_GAP_BLE_PASSKEY_NOTIF_EVT ||
           event == ESP_GAP_BLE_NC_REQ_EVT) {
    if (!selected(p->ble_security.key_notif.bd_addr)) {
      rejectLater(p->ble_security.key_notif.bd_addr);
      return;
    }
    portENTER_CRITICAL(&mux);
    info.pairMode = event == ESP_GAP_BLE_NC_REQ_EVT ? 2 : 1;
    info.passkey = p->ble_security.key_notif.passkey;
    ++info.revision;
    portEXIT_CRITICAL(&mux);
  } else if (event == ESP_GAP_BLE_PASSKEY_REQ_EVT) {
    esp_ble_passkey_reply(p->ble_security.ble_req.bd_addr, false, 0);
    status("Unsupported PIN method. Try another pairing mode.");
  } else if (event == ESP_GAP_BLE_AUTH_CMPL_EVT) {
    bool ours = selected(p->ble_security.auth_cmpl.bd_addr);
    portENTER_CRITICAL(&mux);
    if (ours) {
      info.pairMode = 0;
      ++info.revision;
    }
    portEXIT_CRITICAL(&mux);
    if (ours && !p->ble_security.auth_cmpl.success)
      status("Pairing failed. Select device to retry, or Forget bonds.");
  }
}
void hid(void *, esp_event_base_t, int32_t event, void *arg) {
  auto *p = (esp_hidh_event_data_t *)arg;
  if (event == ESP_HIDH_OPEN_EVENT) {
    if (p->open.status != ESP_OK)
      return;
    auto *dev = p->open.dev;
    const uint8_t *b = esp_hidh_dev_bda_get(dev);
    if (!b)
      return;
    portENTER_CRITICAL(&mux);
    bool accept =
        info.enabled && pending && pendingEpoch == epoch && same(pairing, b);
    unsigned i = 0;
    while (i < 2 && peers[i].dev)
      ++i;
    portEXIT_CRITICAL(&mux);
    if (!accept || i == 2) {
      rejectLater(b);
      return;
    }
    Peer &peer = peers[i];
    memset(&peer, 0, sizeof(peer));
    size_t count = 0;
    esp_hid_raw_report_map_t *maps = nullptr;
    if (esp_hidh_dev_report_maps_get(dev, &count, &maps) == ESP_OK &&
        count <= 2)
      for (unsigned j = 0; j < count; j++)
        if (freslest_hid_map_parse(&peer.maps[j], maps[j].data, maps[j].len))
          peer.kinds[j] = peer.maps[j].kind;
    unsigned kind = peer.kinds[0] | peer.kinds[1];
    if (!kind) {
      portENTER_CRITICAL(&mux);
      int s = savedIndex(b);
      if (s >= 0)
        retry[s].paused = true;
      pending = false;
      info.busy = false;
      info.pairMode = 0;
      ++info.revision;
      portEXIT_CRITICAL(&mux);
      status("Unsupported HID report layout.");
      rejectLater(b);
      return;
    }
    const char *name = esp_hidh_dev_name_get(dev);
    portENTER_CRITICAL(&mux);
    if (!info.enabled || !pending || pendingEpoch != epoch ||
        !same(pairing, b)) {
      portEXIT_CRITICAL(&mux);
      rejectLater(b);
      return;
    }
    remember(b, kind, name);
    peer.dev = dev;
    peer.epoch = epoch;
    memcpy(peer.address, b, 6);
    pending = false;
    info.busy = false;
    info.pairMode = 0;
    refreshList();
    portEXIT_CRITICAL(&mux);
    if (kind & 1)
      freslest_hid_external_connect(i * 2, 1);
    if (kind & 2)
      freslest_hid_external_connect(i * 2 + 1, 2);
    esp_ble_conn_update_params_t params{};
    memcpy(params.bda, b, 6);
    params.min_int = 12;
    params.max_int = 40;
    params.latency = 6;
    params.timeout = 600;
    esp_ble_gap_update_conn_params(&params);
    status("Connected. Saved device reconnects automatically.");
    Serial.printf("BLE connected %02X:%02X:%02X; kind=%u\n", b[3], b[4], b[5],
                  kind);
  } else if (event == ESP_HIDH_INPUT_EVENT) {
    if (!enabled() || p->input.map_index >= 2)
      return;
    for (unsigned i = 0; i < 2; i++)
      if (peers[i].dev == p->input.dev && peers[i].epoch == epoch) {
        Peer &peer = peers[i];
        unsigned map = p->input.map_index;
        freslest_hid_decoded_t decoded;
        if (!peer.kinds[map] ||
            !freslest_hid_map_decode(&peer.maps[map], p->input.report_id,
                                     p->input.data, p->input.length, &decoded))
          return;
        uint8_t keys[32] = {0}, buttons = 0;
        for (unsigned j = 0; j < 2; j++)
          for (unsigned k = 0; k < peer.maps[j].reports; k++) {
            auto &r = peer.maps[j].report[k];
            for (unsigned b = 0; b < 32; b++)
              keys[b] |= r.keys[b];
            buttons |= r.buttons;
          }
        if ((peer.kinds[0] | peer.kinds[1]) & 1)
          freslest_hid_external_keys(i * 2, keys);
        if ((peer.kinds[0] | peer.kinds[1]) & 2)
          freslest_hid_external_mouse(i * 2 + 1, buttons, decoded.dx,
                                      decoded.dy);
        return;
      }
  } else if (event == ESP_HIDH_CLOSE_EVENT) {
    for (unsigned i = 0; i < 2; i++)
      if (peers[i].dev == p->close.dev) {
        freslest_hid_external_disconnect(i * 2);
        freslest_hid_external_disconnect(i * 2 + 1);
        uint32_t now = millis();
        portENTER_CRITICAL(&mux);
        int s = savedIndex(peers[i].address);
        if (s >= 0) {
          retry[s].seen = false;
          retry[s].failures = 0;
          retry[s].due = now + 750;
        }
        peers[i].dev = nullptr;
        ++info.disconnects;
        info.lastCloseReason = p->close.reason;
        nextScan = now + 250;
        refreshList();
        portEXIT_CRITICAL(&mux);
        if (enabled())
          status("Disconnected. Wake the mouse; automatic reconnect is on.");
        Serial.printf("BLE closed reason=0x%x\n", p->close.reason);
      }
    // The SDK owns/frees the device. Worker never retains or closes a dev
    // pointer.
  }
}
bool memoryReady(size_t minimum,size_t block) {
  // BTU creates internal-RAM FreeRTOS tasks/queues even with PSRAM enabled.
  // IDF 5.4.4's allocation-failure cleanup can iterate an uninitialized
  // L2CAP pool (list_begin(NULL)); reject pressure before entering that path.
  size_t freeInternal=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  size_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  Serial.printf("BLE memory: INTERNAL free=%u largest=%u; PSRAM free=%u\n",(unsigned)freeInternal,(unsigned)largest,
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(freeInternal<minimum || largest<block){
    portENTER_CRITICAL(&mux);info.enabled=false;info.ready=false;++info.revision;portEXIT_CRITICAL(&mux);
    status("Low internal RAM. Close apps or restart, then retry Bluetooth.");
    return false;
  }
  return true;
}
bool initialize() {
  if(initialized)return true;
  if(!memoryReady(65536u,12288u))return false;
  esp_err_t e = ESP_OK;
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    esp_bt_controller_config_t c = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    e = esp_bt_controller_init(&c);
  }
  if (e == ESP_OK &&
      esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED)
    e = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (e == ESP_OK &&
      esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED)
    e = esp_bluedroid_init();
  if (e == ESP_OK && esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    // Recheck after controller/BTC allocations, immediately before BTU starts.
    if(!memoryReady(32768u,8192u))return false;
    e = esp_bluedroid_enable();
  }
  if (e == ESP_OK)
    e = esp_ble_gap_register_callback(gap);
  if (e == ESP_OK)
    e = esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler);
  esp_hidh_config_t c = {};
  c.callback = hid;
  c.event_stack_size = 8192;
  if (e == ESP_OK)
    e = esp_hidh_init(&c);
  uint8_t auth = ESP_LE_AUTH_BOND, io = ESP_IO_CAP_OUT,
          key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK, keySize = 16;
  if (e == ESP_OK)
    e = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth, 1);
  if (e == ESP_OK)
    e = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &io, 1);
  if (e == ESP_OK)
    e = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &key, 1);
  if (e == ESP_OK)
    e = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &key, 1);
  if (e == ESP_OK)
    e = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &keySize, 1);
  if (e != ESP_OK) {
    Serial.printf("BLE HID init: %s\n", esp_err_to_name(e));
    status("BLE init failed. Restart to retry.");
    return false;
  }
  initialized = true;
  loadSaved();
  if (esp_ble_gap_config_local_privacy(true) != ESP_OK)
    privacyReady = true;
  portENTER_CRITICAL(&mux);
  info.ready = true;
  ++info.revision;
  portEXIT_CRITICAL(&mux);
  return true;
}
void scan(bool manual = false) {
  if (!initialized || !enabled())
    return;
  portENTER_CRITICAL(&mux);
  if (info.busy || info.scanning || scanRequested) {
    portEXIT_CRITICAL(&mux);
    return;
  }
  unsigned count = 0;
  for (unsigned i = 0; i < info.count; i++)
    if (info.devices[i].connected || info.devices[i].remembered)
      info.devices[count++] = info.devices[i];
  info.count = count;
  scanRequested = true;
  scanSeconds = manual ? 6 : 2;
  ++info.revision;
  portEXIT_CRITICAL(&mux);
  esp_ble_scan_params_t p{};
  p.scan_type = BLE_SCAN_TYPE_ACTIVE;
  p.own_addr_type =
      privateAddress ? BLE_ADDR_TYPE_RPA_PUBLIC : BLE_ADDR_TYPE_PUBLIC;
  p.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  p.scan_interval = 0x80;
  p.scan_window = 0x20;
  p.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  if (esp_ble_gap_set_scan_params(&p) != ESP_OK) {
    portENTER_CRITICAL(&mux);
    scanRequested = false;
    portEXIT_CRITICAL(&mux);
    status("Cannot scan. Press FIRE to retry.");
  }
}
void connectDevice(FreslestBleDevice d, bool manual) {
  uint32_t gen;
  portENTER_CRITICAL(&mux);
  gen = epoch;
  bool on = info.enabled;
  int s = savedIndex(d.address);
  bool already = linked(d.address);
  if (s >= 0 && manual)
    retry[s].paused = already;
  if (already) {
    info.busy = false;
    info.pairMode = 0;
    ++info.revision;
  }
  if (on && !already) {
    pending = true;
    pendingEpoch = epoch;
    pendingDevice = d;
    memcpy(pairing, d.address, 6);
    info.busy = true;
    info.pairMode = 0;
    handshakeDeadline = 0;
    ++info.attempts;
    ++info.revision;
  }
  portEXIT_CRITICAL(&mux);
  if (!on)
    return;
  if (already) {
    esp_ble_gap_disconnect(d.address);
    status("Disconnected by user. Select again to reconnect.");
    return;
  }
  esp_ble_gap_stop_scanning();
  for (unsigned i = 0; i < 35; i++) {
    portENTER_CRITICAL(&mux);
    bool onNow = info.enabled, scanning = info.scanning || scanRequested;
    portEXIT_CRITICAL(&mux);
    if (!onNow || !scanning)
      break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  portENTER_CRITICAL(&mux);
  on = info.enabled && epoch == gen;
  portEXIT_CRITICAL(&mux);
  if (!on)
    return;
  status(manual ? "Connecting... follow any pairing code."
                : "Reconnecting saved keyboard / mouse...");
  // The returned SDK pointer may already have a queued OPEN/CLOSE event.
  // Never dereference it from this task, including cancellation paths.
  bool opened = esp_hidh_dev_open(d.address, ESP_HID_TRANSPORT_BLE,
                                  d.addressType) != nullptr;
  uint32_t now = millis();
  portENTER_CRITICAL(&mux);
  bool current = pending && pendingEpoch == gen && epoch == gen;
  if (current && !opened) {
    retryFailed(s, now);
    pending = false;
    info.busy = false;
    info.pairMode = 0;
    ++info.revision;
  } else if (current)
    handshakeDeadline = now + 5000;
  bool stale = epoch != gen || !info.enabled;
  portEXIT_CRITICAL(&mux);
  if (stale)
    esp_ble_gap_disconnect(d.address);
  else if (!opened)
    status("Not connected. Wake device; saved peers retry automatically.");
}
void maintenance(uint32_t now) {
  uint8_t rejected[2][6];
  unsigned n;
  portENTER_CRITICAL(&mux);
  n = rejectCount;
  memcpy(rejected, rejects, sizeof(rejected));
  rejectCount = 0;
  portEXIT_CRITICAL(&mux);
  for (unsigned i = 0; i < n; i++)
    esp_ble_gap_disconnect(rejected[i]);
  persist(now);
  if (!initialized || !enabled())
    return;
  uint8_t timedOut[6];
  bool timeout = false;
  FreslestBleDevice candidate{};
  bool connect = false, search = false;
  portENTER_CRITICAL(&mux);
  if (pending && handshakeDeadline && due(now, handshakeDeadline)) {
    memcpy(timedOut, pairing, 6);
    retryFailed(savedIndex(pairing), now);
    pending = false;
    info.busy = false;
    info.pairMode = 0;
    ++info.revision;
    timeout = true;
  }
  if (!info.busy && liveCount() < 2) {
    for (unsigned i = 0; i < saved.count; i++)
      if (!linked(saved.peer[i].address) && !retry[i].paused) {
        search = true;
        if (retry[i].seen && (uint32_t)(now - retry[i].seenAt) < 8000 &&
            due(now, retry[i].due)) {
          memcpy(candidate.address, saved.peer[i].address, 6);
          candidate.addressType = saved.peer[i].type;
          strlcpy(candidate.name, saved.peer[i].name, sizeof(candidate.name));
          connect = true;
          break;
        }
      }
  }
  bool canScan = !info.busy && !info.scanning && !scanRequested &&
                 privacyReady && due(now, nextScan);
  if (canScan)
    nextScan = now + 6000;
  portEXIT_CRITICAL(&mux);
  if (timeout) {
    esp_ble_gap_disconnect(timedOut);
    status("HID setup timed out. Automatic retry remains on.");
  }
  if (connect)
    connectDevice(candidate, false);
  else if (canScan && search)
    scan();
}
void process(Command c) {
  if (c.op == START) {
    if (enabled() && initialize()) {
      portENTER_CRITICAL(&mux);
      for (auto &r : retry)
        r = {false, false, 0, 0, millis()};
      nextScan = millis();
      portEXIT_CRITICAL(&mux);
      status("Saved peers restore automatically. Wake your mouse.");
      scan(true);
    }
  } else if (c.op == STOP || c.op == FORGET) {
    if (c.op == FORGET && !initialized)
      initialize();
    if (initialized)
      esp_ble_gap_stop_scanning();
    uint8_t b[2][6];
    unsigned n = 0;
    portENTER_CRITICAL(&mux);
    for (auto &p : peers)
      if (p.dev)
        memcpy(b[n++], p.address, 6);
    portEXIT_CRITICAL(&mux);
    for (unsigned i = 0; i < n; i++)
      esp_ble_gap_disconnect(b[i]);
    if (c.op == FORGET && initialized) {
      int count = esp_ble_get_bond_device_num();
      if (count > 0 && count <= 32) {
        auto *bonds = (esp_ble_bond_dev_t *)calloc((size_t)count,
                                                   sizeof(esp_ble_bond_dev_t));
        if (bonds) {
          if (esp_ble_get_bond_device_list(&count, bonds) == ESP_OK)
            for (int j = 0; j < count; j++)
              esp_ble_remove_bond_device(bonds[j].bd_addr);
          free(bonds);
        }
      }
      portENTER_CRITICAL(&mux);
      saved.count = 0;
      memset(retry, 0, sizeof(retry));
      dirty = true;
      saveAfter = 0;
      info.count = 0;
      refreshList();
      portEXIT_CRITICAL(&mux);
      persist(millis());
      status("Bonds removed. Put devices in pairing mode.");
    } else
      status("Bluetooth is off.");
  } else if (c.op == SCAN)
    scan(true);
  else if (c.op == CONNECT) {
    if (enabled() && initialized)
      connectDevice(c.device, true);
    else {
      portENTER_CRITICAL(&mux);
      info.busy = false;
      ++info.revision;
      portEXIT_CRITICAL(&mux);
    }
  }
}
void worker(void *) {
  for (;;) {
    Command c;
    if (xQueueReceive(commands, &c, pdMS_TO_TICKS(200)) == pdTRUE)
      process(c);
    maintenance(millis());
  }
}
bool send(Command c) {
  if (!commands) {
    commands = xQueueCreate(8, sizeof(Command));
    if (!commands || xTaskCreatePinnedToCore(worker, "ble_hid", 6144, nullptr,
                                             3, nullptr, 0) != pdPASS) {
      if (commands) {
        vQueueDelete(commands);
        commands = nullptr;
      }
      status("BLE task unavailable.");
      return false;
    }
  }
  if (xQueueSend(commands, &c, 0) == pdTRUE)
    return true;
  if (c.op == STOP) {
    xQueueReset(commands);
    return xQueueSend(commands, &c, 0) == pdTRUE;
  }
  status("Bluetooth is busy. Please retry.");
  return false;
}
} // namespace
FreslestBleInfo freslestBleInfo() {
  portENTER_CRITICAL(&mux);
  auto s = info;
  portEXIT_CRITICAL(&mux);
  return s;
}
void freslestBleStart() {
  if (freslestBleInfo().enabled)
    return;
  portENTER_CRITICAL(&mux);
  info.enabled = true;
  ++epoch;
  ++info.revision;
  portEXIT_CRITICAL(&mux);
  if (!send({START, {}})) {
    portENTER_CRITICAL(&mux);
    info.enabled = false;
    portEXIT_CRITICAL(&mux);
  }
}
void freslestBleStop() {
  uint8_t b[6];
  portENTER_CRITICAL(&mux);
  info.enabled = false;
  info.busy = false;
  info.pairMode = 0;
  bool wasPending = pending;
  memcpy(b, pairing, 6);
  pending = false;
  ++epoch;
  refreshList();
  portEXIT_CRITICAL(&mux);
  for (unsigned i = 0; i < 4; i++)
    freslest_hid_external_disconnect(i);
  if (wasPending)
    esp_ble_gap_disconnect(b);
  send({STOP, {}});
}
void freslestBleScan() {
  auto s = freslestBleInfo();
  if (s.enabled && s.ready && !s.busy && !s.scanning)
    send({SCAN, {}});
}
void freslestBleConnect(unsigned i) {
  auto s = freslestBleInfo();
  if (i >= s.count || !s.enabled || !s.ready || s.busy)
    return;
  portENTER_CRITICAL(&mux);
  info.busy = true;
  ++info.revision;
  portEXIT_CRITICAL(&mux);
  if (!send({CONNECT, s.devices[i]})) {
    portENTER_CRITICAL(&mux);
    info.busy = false;
    ++info.revision;
    portEXIT_CRITICAL(&mux);
  }
}
void freslestBleForget() {
  auto s = freslestBleInfo();
  if (!s.busy)
    send({FORGET, {}});
}
void freslestBlePairReply(bool accept) {
  uint8_t b[6];
  portENTER_CRITICAL(&mux);
  unsigned mode = info.pairMode;
  memcpy(b, pairing, 6);
  info.pairMode = 0;
  ++info.revision;
  portEXIT_CRITICAL(&mux);
  if (mode == 2)
    esp_ble_confirm_reply(b, accept);
  else if (mode == 1 && !accept)
    esp_ble_gap_disconnect(b);
}
void freslestBleDiagnostics() {
  auto s = freslestBleInfo();
  Serial.printf("BLE enabled=%u ready=%u connected=%u scan=%u busy=%u "
                "attempts=%lu closes=%lu last-reason=0x%x; %s\n",
                s.enabled, s.ready, s.connected, s.scanning, s.busy,
                (unsigned long)s.attempts, (unsigned long)s.disconnects,
                s.lastCloseReason, s.status);
}
