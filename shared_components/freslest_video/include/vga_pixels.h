// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { VGA_WIDTH=640, VGA_HEIGHT=480 };
bool vga_convert(uint8_t *dest,const void *src,int width,int height,
                 const uint32_t *palette,bool swapped,bool cursor,int x,int y);

typedef struct {int x,y,w,h;} vga_viewport_t;
vga_viewport_t vga_viewport(unsigned percent);
bool vga_convert_fit(uint8_t *dest,const void *src,int width,int height,const uint32_t *palette,bool swapped,bool cursor,int x,int y,unsigned percent);
void vga_doom_palette(const uint8_t *rgb888);
bool vga_convert_doom(uint8_t *dest,const uint8_t *indices,bool cursor,int x,int y,unsigned percent);

// Identical channel packing to the supplied working V1.0 core.
static inline uint8_t vga_pack_rgb332(uint8_t r,uint8_t g,uint8_t b){
 return (uint8_t)((r&0xe0)|((g&0xe0)>>3)|(b>>6));
}
