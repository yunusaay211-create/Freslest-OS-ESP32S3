// SPDX-License-Identifier: Apache-2.0
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freslest_hid_state.h"
#include "usb/hid_host.h"
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static freslest_hid_state_t state;
static hid_host_device_handle_t handles[FRESLEST_HID_USB_SLOTS];
static QueueHandle_t arrivals;
static bool started;
static const char *status = "Not started";
static void input_event(hid_host_device_handle_t handle,
                        hid_host_interface_event_t event, void *arg) {
  unsigned slot = (unsigned)(uintptr_t)arg;
  if (slot >= FRESLEST_HID_USB_SLOTS)
    return;
  if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
    uint8_t data[64];
    size_t size = 0;
    if (hid_host_device_get_raw_input_report_data(handle, data, sizeof(data),
                                                  &size) == ESP_OK) {
      portENTER_CRITICAL(&lock);
      if (handles[slot] == handle)
        freslest_hid_state_report(&state, slot, data, size);
      portEXIT_CRITICAL(&lock);
    }
  } else if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED ||
             event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
    portENTER_CRITICAL(&lock);
    if (handles[slot] == handle) {
      freslest_hid_state_disconnect(&state, slot);
      if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED)
        handles[slot] = NULL;
    }
    portEXIT_CRITICAL(&lock);
    if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED)
      hid_host_device_close(handle);
    else
      ESP_LOGW("USB input", "Transfer error: reconnect input device");
  }
}
static void connected(hid_host_device_handle_t h, hid_host_driver_event_t event,
                      void *arg) {
  (void)arg;
  if (event == HID_HOST_DRIVER_EVENT_CONNECTED &&
      xQueueSend(arrivals, &h, 0) != pdTRUE)
    ESP_LOGW("USB input", "Connection queue full");
}
static void worker(void *unused) {
  (void)unused;
  hid_host_device_handle_t handle;
  for (;;) {
    if (xQueueReceive(arrivals, &handle, pdMS_TO_TICKS(20)) != pdTRUE)
      continue;
    hid_host_dev_params_t p;
    if (hid_host_device_get_params(handle, &p) != ESP_OK)
      continue;
    if (p.sub_class != HID_SUBCLASS_BOOT_INTERFACE ||
        (p.proto != HID_PROTOCOL_KEYBOARD && p.proto != HID_PROTOCOL_MOUSE)) {
      ESP_LOGW("USB input", "Boot keyboard/mouse interface required");
      continue;
    }
    portENTER_CRITICAL(&lock);
    unsigned slot = 0;
    while (slot < FRESLEST_HID_USB_SLOTS && handles[slot])
      ++slot;
    portEXIT_CRITICAL(&lock);
    if (slot == FRESLEST_HID_USB_SLOTS)
      continue;
    hid_host_device_config_t config = {.callback = input_event,
                                       .callback_arg = (void *)(uintptr_t)slot};
    esp_err_t e = hid_host_device_open(handle, &config);
    if (e != ESP_OK)
      continue;
    portENTER_CRITICAL(&lock);
    handles[slot] = handle;
    portEXIT_CRITICAL(&lock);
    e = hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT);
    if (e == ESP_OK && p.proto == HID_PROTOCOL_KEYBOARD)
      hid_class_request_set_idle(handle, 0, 0);
    portENTER_CRITICAL(&lock);
    bool attached = handles[slot] == handle;
    if (attached)
      freslest_hid_state_connect(&state, slot, p.proto);
    portEXIT_CRITICAL(&lock);
    if (!attached)
      continue;
    if (e == ESP_OK)
      e = hid_host_device_start(handle);
    if (e != ESP_OK) {
      portENTER_CRITICAL(&lock);
      if (handles[slot] == handle)
        freslest_hid_state_disconnect(&state, slot);
      portEXIT_CRITICAL(&lock);
      hid_host_device_close(handle);
      ESP_LOGW("USB input", "Start failed: %s", esp_err_to_name(e));
    }
  }
}
static void daemon_task(void *unused) {
  (void)unused;
  for (;;) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(pdMS_TO_TICKS(20), &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();
    vTaskDelay(1);
  }
}
bool freslest_hid_start(void) {
  if (started)
    return true;
  arrivals = xQueueCreate(8, sizeof(hid_host_device_handle_t));
  if (!arrivals) {
    status = "Input queue unavailable";
    return false;
  }
  usb_host_config_t host = {.intr_flags = ESP_INTR_FLAG_LEVEL1};
  esp_err_t e = usb_host_install(&host);
  if (e != ESP_OK) {
    status = "USB host unavailable";
    vQueueDelete(arrivals);
    arrivals = NULL;
    return false;
  }
  TaskHandle_t daemon = NULL, consumer = NULL;
  if (xTaskCreatePinnedToCore(daemon_task, "hid_usb", 4096, NULL, 4, &daemon,
                              0) != pdPASS ||
      xTaskCreatePinnedToCore(worker, "hid_open", 4096, NULL, 3, &consumer,
                              0) != pdPASS) {
    if (daemon)
      vTaskDelete(daemon);
    if (consumer)
      vTaskDelete(consumer);
    usb_host_uninstall();
    vQueueDelete(arrivals);
    arrivals = NULL;
    status = "Input task unavailable";
    return false;
  }
  hid_host_driver_config_t config = {.create_background_task = true,
                                     .task_priority = 4,
                                     .stack_size = 4096,
                                     .core_id = 0,
                                     .callback = connected};
  e = hid_host_install(&config);
  if (e != ESP_OK) {
    vTaskDelete(consumer);
    vTaskDelete(daemon);
    usb_host_uninstall();
    vQueueDelete(arrivals);
    arrivals = NULL;
    status = "HID driver unavailable";
    return false;
  }
  started = true;
  status = "USB host: boot keyboard / mouse";
  return true;
}
void freslest_hid_snapshot(freslest_hid_snapshot_t *out) {
  portENTER_CRITICAL(&lock);
  freslest_hid_state_take(&state, out, true);
  portEXIT_CRITICAL(&lock);
}
void freslest_hid_peek(freslest_hid_snapshot_t *out) {
  portENTER_CRITICAL(&lock);
  freslest_hid_state_take(&state, out, false);
  portEXIT_CRITICAL(&lock);
}
const char *freslest_hid_status(void) { return status; }
void freslest_hid_external_connect(unsigned i,unsigned kind){
 if(i>=4){return;}portENTER_CRITICAL(&lock);
 freslest_hid_state_connect(&state,i+4,kind);portEXIT_CRITICAL(&lock);
}
void freslest_hid_external_disconnect(unsigned i){
 if(i>=4){return;}portENTER_CRITICAL(&lock);
 freslest_hid_state_disconnect(&state,i+4);portEXIT_CRITICAL(&lock);
}
void freslest_hid_external_keys(unsigned i,const uint8_t keys[32]){
 if(i>=4){return;}portENTER_CRITICAL(&lock);
 freslest_hid_state_keys(&state,i+4,keys);portEXIT_CRITICAL(&lock);
}
void freslest_hid_external_mouse(unsigned i,uint8_t b,int dx,int dy){
 if(i>=4){return;}portENTER_CRITICAL(&lock);
 freslest_hid_state_mouse(&state,i+4,b,dx,dy);portEXIT_CRITICAL(&lock);
}
