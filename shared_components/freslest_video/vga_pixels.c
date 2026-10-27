// SPDX-License-Identifier: MIT
// RGB565/indexed source -> physical RGB332, same bit order as V1.0 VGA core.
#include "vga_pixels.h"
#include "freslest_cursor.h"
#include <stddef.h>
static uint8_t from565(uint16_t c){return ((c>>8)&0xe0)|((c>>6)&0x1c)|((c>>3)&3);}
bool vga_convert(uint8_t *dest,const void *source,int w,int h,const uint32_t *pal,bool swap,bool cursor,int cx,int cy){
 if(!dest||!source||w<1||h<1||w>480||h>360)return false;
 // OS/Doom each have one video producer. Keep the coordinate scratch out of
 // the small OS main-task stack; no DMA consumer reads this lookup table.
 static uint16_t columns[VGA_WIDTH];
 for(int x=0;x<VGA_WIDTH;x++)columns[x]=x*w/VGA_WIDTH;
 for(int y=0;y<VGA_HEIGHT;y++){
  int sy=y*h/VGA_HEIGHT;uint8_t *out=dest+(size_t)y*VGA_WIDTH;
  for(int x=0;x<VGA_WIDTH;x++){
   int sx=columns[x];size_t i=(size_t)sy*w+sx;uint8_t c;
   if(pal){uint32_t rgb=pal[((const uint8_t*)source)[i]];c=(rgb&0xe0)|((rgb>>11)&0x1c)|((rgb>>22)&3);}
   else{uint16_t rgb=((const uint16_t*)source)[i];if(swap)rgb=(rgb>>8)|(rgb<<8);c=from565(rgb);}
   if(cursor&&sx>=cx&&sy>=cy&&sx<cx+FRESLEST_CURSOR_W&&sy<cy+FRESLEST_CURSOR_H){
    unsigned p=freslest_cursor_pixel(sx-cx,sy-cy);if(p)c=p==1?0:p==2?255:from565(0x057f);
   }
   out[x]=c;
  }
 }
 return true;
}
