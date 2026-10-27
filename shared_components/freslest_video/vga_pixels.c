// SPDX-License-Identifier: MIT
// RGB565/indexed source -> physical RGB332, same bit order as V1.0 VGA core.
#include "vga_pixels.h"
#include "freslest_cursor.h"
#include <stddef.h>
#include <string.h>
static uint8_t from565(uint16_t c){return ((c>>8)&0xe0)|((c>>6)&0x1c)|((c>>3)&3);}
vga_viewport_t vga_viewport(unsigned percent){
 if(percent<80||percent>100||percent%5)percent=90;
 int w=VGA_WIDTH*percent/100,h=VGA_HEIGHT*percent/100;
 return (vga_viewport_t){(VGA_WIDTH-w)/2,(VGA_HEIGHT-h)/2,w,h};
}
bool vga_convert_fit(uint8_t *dest,const void *source,int w,int h,const uint32_t *pal,bool swap,bool cursor,int cx,int cy,unsigned percent){
 if(!dest||!source||w<1||h<1||w>480||h>360)return false;
 // OS/Doom each have one video producer. Keep the coordinate scratch out of
 // the small OS main-task stack; no DMA consumer reads this lookup table.
 static uint16_t columns[VGA_WIDTH];
 static uint8_t colors[256];
 if(pal)for(unsigned i=0;i<256;i++){
  uint32_t rgb=pal[i];colors[i]=vga_pack_rgb332(rgb,rgb>>8,rgb>>16);
 }
 vga_viewport_t view=vga_viewport(percent);
 // Clear only safe-area borders; the active rectangle is fully overwritten.
 memset(dest,0,(size_t)view.y*VGA_WIDTH);
 memset(dest+(size_t)(view.y+view.h)*VGA_WIDTH,0,(size_t)view.y*VGA_WIDTH);
 for(int x=0;x<view.w;x++)columns[x]=x*w/view.w;
 for(int y=0;y<view.h;y++){
  int sy=y*h/view.h;uint8_t *row=dest+(size_t)(y+view.y)*VGA_WIDTH;
  memset(row,0,view.x);memset(row+view.x+view.w,0,view.x);
  uint8_t *out=row+view.x;
  for(int x=0;x<view.w;x++){
   int sx=columns[x];size_t i=(size_t)sy*w+sx;uint8_t c;
   if(pal)c=colors[((const uint8_t*)source)[i]];
   else{uint16_t rgb=((const uint16_t*)source)[i];if(swap)rgb=(rgb>>8)|(rgb<<8);c=from565(rgb);}
   if(cursor&&sx>=cx&&sy>=cy&&sx<cx+FRESLEST_CURSOR_W&&sy<cy+FRESLEST_CURSOR_H){
    unsigned p=freslest_cursor_pixel(sx-cx,sy-cy);if(p)c=p==1?0:p==2?255:from565(0x057f);
   }
   out[x]=c;
  }
 }
 return true;
}

bool vga_convert(uint8_t *dest,const void *src,int w,int h,const uint32_t *pal,bool swap,bool cursor,int x,int y){
 return vga_convert_fit(dest,src,w,h,pal,swap,cursor,x,y,100);
}
