// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { VGA_WIDTH=640, VGA_HEIGHT=480 };
bool vga_convert(uint8_t *dest,const void *src,int width,int height,
                 const uint32_t *palette,bool swapped,bool cursor,int x,int y);
