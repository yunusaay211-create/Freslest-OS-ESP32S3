// SPDX-License-Identifier: MIT
// Doom-only indexed protocol: original PLAYPAL RGB888 -> cached RGB332.
// No LCD byte swapping, RGB565 round trip, or temporal color alternation.
#include "vga_pixels.h"
#include "freslest_cursor.h"
#include <string.h>
static uint8_t colors[16][256];
static bool palette_ready;
static uint8_t channel(uint8_t v,unsigned levels,unsigned rank){
 unsigned scaled=v*levels,base=scaled/255;
 unsigned count=((scaled%255)*16+127)/255;
 return base+(rank<count);
}
void vga_doom_palette(const uint8_t *rgb){
 if(!rgb)return;
 static const uint8_t ranks[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
 for(unsigned phase=0;phase<16;phase++)for(unsigned i=0;i<256;i++){
  unsigned rank=ranks[phase];const uint8_t *p=rgb+i*3;
  colors[phase][i]=(channel(p[0],7,rank)<<5)|(channel(p[1],7,rank)<<2)|channel(p[2],3,rank);
 }
 palette_ready=true;
}
bool vga_convert_doom(uint8_t *dest,const uint8_t *src,bool cursor,int cx,int cy,unsigned percent){
 if(!dest||!src||!palette_ready)return false;
 vga_viewport_t view=vga_viewport(percent);
 static uint16_t columns[VGA_WIDTH];
 for(int x=0;x<view.w;x++)columns[x]=x*320/view.w;
 memset(dest,0,(size_t)view.y*VGA_WIDTH);
 memset(dest+(size_t)(view.y+view.h)*VGA_WIDTH,0,(size_t)view.y*VGA_WIDTH);
 for(int y=0;y<view.h;y++){
  int sy=y*240/view.h;uint8_t *row=dest+(size_t)(view.y+y)*VGA_WIDTH;
  memset(row,0,view.x);memset(row+view.x+view.w,0,view.x);
  uint8_t *out=row+view.x;
  for(int x=0;x<view.w;x++){
   int sx=columns[x];unsigned phase=(sy&3)*4+(sx&3);
   uint8_t c=colors[phase][src[sy*320+sx]];
   if(cursor&&sx>=cx&&sy>=cy&&sx<cx+FRESLEST_CURSOR_W&&sy<cy+FRESLEST_CURSOR_H){
    unsigned p=freslest_cursor_pixel(sx-cx,sy-cy);if(p)c=p==1?0:p==2?255:0x17;
   }
   out[x]=c;
  }
 }
 return true;
}
