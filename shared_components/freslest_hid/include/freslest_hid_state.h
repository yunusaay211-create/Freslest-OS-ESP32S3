#pragma once
#include "freslest_hid.h"
typedef struct {
  uint8_t kind, keys[32], buttons;
} freslest_hid_slot_t;
typedef struct {
  freslest_hid_slot_t slot[FRESLEST_HID_SLOTS];
  freslest_hid_snapshot_t state;
} freslest_hid_state_t;
void freslest_hid_state_connect(freslest_hid_state_t *, unsigned, unsigned);
void freslest_hid_state_disconnect(freslest_hid_state_t *, unsigned);
bool freslest_hid_state_report(freslest_hid_state_t *, unsigned,
                               const uint8_t *, size_t);
void freslest_hid_state_take(freslest_hid_state_t *, freslest_hid_snapshot_t *,
                             bool);
void freslest_hid_state_keys(freslest_hid_state_t *, unsigned, const uint8_t[32]);
void freslest_hid_state_mouse(freslest_hid_state_t *, unsigned, uint8_t, int, int);
