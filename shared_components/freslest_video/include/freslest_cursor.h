// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#define FRESLEST_CURSOR_W 13
#define FRESLEST_CURSOR_H 19
// Transparent bitmap sprite. 1 = dark outline, 2 = white, 3 = blue accent.
// Hotspot is (0,0); the base framebuffer is never modified by the LCD overlay.
static inline unsigned freslest_cursor_pixel(int x,int y) {
 static const uint16_t outline[19]={1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,511,883,867,1728,1728,768};
 static const uint16_t white[19]={0,0,2,6,14,30,62,126,254,510,1022,2046,62,110,198,196,384,256,0};
 static const uint16_t blue[19]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,16,48,48,0};
 if(x<0||x>=13||y<0||y>=19)return 0;
 unsigned b=1u<<x;
 return (blue[y]&b)?3:(white[y]&b)?2:(outline[y]&b)?1:0;
}
