// SPDX-License-Identifier: MIT
// Doom owns the palette cache and the RGB332-to-DMA-word transfer contract.
#include "doom_rgb332.h"
#include "vga_pixels.h"
#include "freslest_cursor.h"
#include <string.h>
static uint16_t palette_words[256];
static uint16_t columns[VGA_WIDTH];
static bool palette_ready;
void doom_rgb332_palette(const uint8_t *rgb){
 if(!rgb)return;
 for(unsigned i=0;i<256;++i)palette_words[i]=(uint16_t)vga_pack_rgb332(rgb[i*3],rgb[i*3+1],rgb[i*3+2]);
 palette_ready=true;
}
bool doom_rgb332_frame(uint16_t *dest,const uint8_t *src,bool cursor,int cx,int cy,unsigned fit){
 if(!dest||!src||!palette_ready)return false;
 vga_viewport_t view=vga_viewport(fit);
 for(int x=0;x<view.w;++x)columns[x]=x*320/view.w;
 memset(dest,0,(size_t)view.y*VGA_WIDTH*sizeof(*dest));
 memset(dest+(size_t)(view.y+view.h)*VGA_WIDTH,0,(size_t)view.y*VGA_WIDTH*sizeof(*dest));
 for(int y=0;y<view.h;++y){
  int sy=y*240/view.h;uint16_t *row=dest+(size_t)(view.y+y)*VGA_WIDTH;
  memset(row,0,(size_t)view.x*sizeof(*row));
  memset(row+view.x+view.w,0,(size_t)view.x*sizeof(*row));
  uint16_t *out=row+view.x;
  for(int x=0;x<view.w;++x){
   int sx=columns[x];uint16_t word=palette_words[src[sy*320+sx]];
   if(cursor&&sx>=cx&&sy>=cy&&sx<cx+FRESLEST_CURSOR_W&&sy<cy+FRESLEST_CURSOR_H){
    unsigned pixel=freslest_cursor_pixel(sx-cx,sy-cy);if(pixel)word=pixel==1?0:pixel==2?255:0x17;
   }
   out[x]=word;
  }
 }
 return true;
}
void doom_rgb332_expand(uint16_t *words,size_t count){
 // Progress views first produce packed RGB332 in the first half of the same
 // buffer. Expand backward so writing a word never destroys unread bytes.
 uint8_t *bytes=(uint8_t*)words;
 while(count--){words[count]=bytes[count];}
}
