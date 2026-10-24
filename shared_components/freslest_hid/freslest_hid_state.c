#include "freslest_hid_state.h"
#include <string.h>
static void aggregate(freslest_hid_state_t *s) {
  uint8_t keys[32] = {0}, buttons = 0;
  s->state.keyboards = s->state.mice = 0;
  for (unsigned i = 0; i < FRESLEST_HID_SLOTS; i++) {
    freslest_hid_slot_t *p = &s->slot[i];
    if (p->kind == 1) {
      ++s->state.keyboards;
      for (unsigned j = 0; j < 32; j++)
        keys[j] |= p->keys[j];
    }
    if (p->kind == 2) {
      ++s->state.mice;
      buttons |= p->buttons;
    }
  }
  for (unsigned j = 0; j < 32; j++) {
    s->state.pressed[j] |= keys[j] & ~s->state.keys[j];
    s->state.keys[j] = keys[j];
  }
  s->state.mouse_pressed |= buttons & ~s->state.mouse_buttons;
  s->state.mouse_buttons = buttons;
}
void freslest_hid_state_connect(freslest_hid_state_t *s, unsigned i,
                                unsigned kind) {
  if (i >= FRESLEST_HID_SLOTS)
    return;
  memset(&s->slot[i], 0, sizeof(s->slot[i]));
  s->slot[i].kind = kind;
  aggregate(s);
}
void freslest_hid_state_disconnect(freslest_hid_state_t *s, unsigned i) {
  if (i >= FRESLEST_HID_SLOTS)
    return;
  memset(&s->slot[i], 0, sizeof(s->slot[i]));
  aggregate(s);
  for (unsigned j = 0; j < 32; j++)
    s->state.pressed[j] &= s->state.keys[j];
  s->state.mouse_pressed &= s->state.mouse_buttons;
  if (!s->state.mice)
    s->state.dx = s->state.dy = 0;
}
static int16_t motion(int16_t old, int delta) {
  int64_t n = (int64_t)old + delta;
  return n > 2047 ? 2047 : n < -2047 ? -2047 : n;
}
void freslest_hid_state_keys(freslest_hid_state_t *s,unsigned i,const uint8_t keys[32]) {
  if(i>=FRESLEST_HID_SLOTS||s->slot[i].kind!=1||!keys)return;
  memcpy(s->slot[i].keys,keys,32);aggregate(s);
}
void freslest_hid_state_mouse(freslest_hid_state_t *s,unsigned i,uint8_t buttons,int dx,int dy) {
  if(i>=FRESLEST_HID_SLOTS||s->slot[i].kind!=2)return;
  s->slot[i].buttons=buttons&7;
  s->state.dx=motion(s->state.dx,dx);s->state.dy=motion(s->state.dy,dy);aggregate(s);
}
bool freslest_hid_state_report(freslest_hid_state_t *s, unsigned i,
                               const uint8_t *d, size_t n) {
  if (i >= FRESLEST_HID_SLOTS || !d)
    return false;
  freslest_hid_slot_t *p = &s->slot[i];
  if (p->kind == 1) {
    if (n != 8)
      return false;
    uint8_t keys[32] = {0};
    for (unsigned j = 2; j < 8; j++)
      if (d[j] && d[j] < 4)
        return false;
    for (unsigned j = 2; j < 8; j++)
      if (d[j])
        keys[d[j] >> 3] |= 1u << (d[j] & 7);
    keys[28] = d[0];
    memcpy(p->keys, keys, 32);
  } else if (p->kind == 2) {
    if (n < 3 || n > 4)
      return false;
    p->buttons = d[0] & 7;
    s->state.dx = motion(s->state.dx, (int8_t)d[1]);
    s->state.dy = motion(s->state.dy, (int8_t)d[2]);
  } else
    return false;
  aggregate(s);
  return true;
}
void freslest_hid_state_take(freslest_hid_state_t *s,
                             freslest_hid_snapshot_t *out, bool consume) {
  *out = s->state;
  if (consume) {
    memset(s->state.pressed, 0, 32);
    s->state.mouse_pressed = 0;
    s->state.dx = s->state.dy = 0;
  }
}
