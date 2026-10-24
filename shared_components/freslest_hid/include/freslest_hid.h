#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FRESLEST_HID_SLOTS 8
#define FRESLEST_HID_USB_SLOTS 4
typedef struct {
  uint8_t keys[32], pressed[32];
  int16_t dx, dy;
  uint8_t mouse_buttons, mouse_pressed, keyboards, mice;
} freslest_hid_snapshot_t;
bool freslest_hid_start(void);
void freslest_hid_snapshot(freslest_hid_snapshot_t *output);
void freslest_hid_peek(freslest_hid_snapshot_t *output);
const char *freslest_hid_status(void);
// Four independent BLE sources, kept separate from the USB interface slots.
void freslest_hid_external_connect(unsigned source, unsigned kind);
void freslest_hid_external_disconnect(unsigned source);
void freslest_hid_external_keys(unsigned source, const uint8_t keys[32]);
void freslest_hid_external_mouse(unsigned source, uint8_t buttons, int dx, int dy);
static inline bool freslest_hid_key(const uint8_t *bits, unsigned usage) {
  return usage < 256 && (bits[usage >> 3] & (1u << (usage & 7)));
}
#ifdef __cplusplus
}
#endif
