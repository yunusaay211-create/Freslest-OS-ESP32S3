// SPDX-License-Identifier: Apache-2.0
#include <freslest_hid.h>
#include <string.h>
#undef false
#undef true
#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_menu.h"
#include "usb_doom.h"
static uint8_t desired[512], previous[512];
static int turnRemainder,pointerX=160,pointerY=120;
static bool pointerVisible;
bool usbDoomPointer(int *x,int *y){if(x)*x=pointerX;if(y)*y=pointerY;return pointerVisible;}
static int limit(int n,int min,int max){return n<min?min:n>max?max:n;}
extern void Freslest_MenuPointer(int x,int y);
bool usbDoomKeyHeld(int k) { return k > 0 && k < 512 && desired[k]; }
static void want(int k) {
  if (k > 0 && k < 512)
    desired[k] = 1;
}
static int mapped(unsigned u) {
  switch (u) {
  case 0x1a:
    return key_up;
  case 0x16:
    return key_down;
  case 4:
    return key_strafeleft;
  case 7:
    return key_straferight;
  case 0x52:
    return menuactive ? key_menu_up : key_up;
  case 0x51:
    return menuactive ? key_menu_down : key_down;
  case 0x50:
    return menuactive ? key_menu_left : key_left;
  case 0x4f:
    return menuactive ? key_menu_right : key_right;
  case 8:
  case 0x2c:
    return key_use;
  case 0x28:
    return key_menu_enter;
  case 0x29:
    return key_escape;
  case 0x2a:
    return key_menu_backspace;
  case 0x2b:
    return key_map;
  case 0xe0:
  case 0xe4:
    return key_fire;
  case 0xe1:
  case 0xe5:
    return key_speed;
  case 0xe2:
  case 0xe6:
    return key_strafe;
  default:
    break;
  }
  if (u >= 4 && u <= 29)
    return 'a' + u - 4;
  if (u >= 30 && u <= 38)
    return '1' + u - 30;
  if (u == 39)
    return '0';
  return 0;
}
void usbDoomPoll(void) {
  freslest_hid_snapshot_t in;
  freslest_hid_snapshot(&in);
  memset(desired, 0, sizeof(desired));
  for (unsigned u = 4; u < 256; u++)
    if (freslest_hid_key(in.keys, u) || freslest_hid_key(in.pressed, u))
      want(mapped(u));
  bool play = gamestate == GS_LEVEL && !menuactive;
  pointerVisible=in.mice&&!play;
  if(!play){
    pointerX=limit(pointerX+in.dx,0,319);pointerY=limit(pointerY+in.dy,0,239);
    if(menuactive&&(in.dx||in.dy||in.mouse_pressed))Freslest_MenuPointer(pointerX,pointerY);
    turnRemainder=0;
  }
  if(!in.mice)turnRemainder=0;
  uint8_t mouse = in.mouse_buttons | in.mouse_pressed;
  if (mouse & 1)
    want(play ? key_fire : key_menu_enter);
  if (mouse & 2)
    want(play ? key_use : key_menu_backspace);
  for (int k = 1; k < 512; k++)
    if (desired[k] != previous[k] && (desired[k] || !gamepadKeyHeld(k))) {
      event_t e = {0};
      e.type = desired[k] ? ev_keydown : ev_keyup;
      e.data1 = k;
      D_PostEvent(&e);
    }
  memcpy(previous, desired, sizeof(previous));
  // Classic Doom horizontal turning; vertical free-look is not part of this
  // engine.
  if(play&&in.mice){
    // Quarter of the old turn rate. Fractional accumulation preserves a one-
    // count motion at low speed; clamp accumulated USB bursts after a stall.
    int total=turnRemainder+limit(in.dx,-256,256);
    int delta=total/4;turnRemainder=total-delta*4;
    if(delta){event_t e={0};e.type=ev_mouse;e.data2=delta;D_PostEvent(&e);}
  }
}
