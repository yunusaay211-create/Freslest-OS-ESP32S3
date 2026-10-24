#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "video_pixels.h"
#include "video_slots.h"
#include "freslest_cursor.h"
int main(void){
 size_t n=VIDEO_DMA_W*VIDEO_DMA_H;
 uint32_t *aligned=malloc(n+64);assert(aligned);uint8_t *wave=(uint8_t*)aligned+32;
 memset(aligned,0xa5,n+64);uint8_t palette[256][4];video_phase_palette(palette);video_prime_wave(wave);
 assert(VIDEO_DMA_W+10==VIDEO_LINE_SAMPLES&&VIDEO_DMA_H+10==VIDEO_FIELD_LINES);
 for(int y=0;y<VIDEO_DMA_H;y++){
  uint8_t *row=wave+y*VIDEO_DMA_W;
  if(y>=248&&y<251){for(int x=0;x<833;x++)assert(!row[x]);assert(row[833]==VIDEO_BLANK);}
  else {for(int x=0;x<67;x++)assert(!row[x]);assert(row[67]==VIDEO_BLANK);
   assert(row[76]==(y&1?37:109)&&row[78]==(y&1?109:37));}
 }
 for(int k=0;k<4;k++)assert(palette[0][k]==VIDEO_BLACK&&palette[255][k]==255);
 uint8_t *frame=malloc(VIDEO_W*VIDEO_H);assert(frame);memset(frame,255,VIDEO_W*VIDEO_H);
 video_encode_wave(wave,frame,palette);
 for(int y=0;y<VIDEO_H;y++)for(int x=0;x<VIDEO_IMAGE_SAMPLES;x++)assert(wave[(y+VIDEO_IMAGE_TOP)*VIDEO_DMA_W+VIDEO_IMAGE_START+x]==255);
 for(int i=0;i<32;i++)assert(((uint8_t*)aligned)[i]==0xa5&&wave[n+i]==0xa5);
 assert(!wave[VIDEO_IMAGE_TOP*VIDEO_DMA_W]&&wave[VIDEO_IMAGE_TOP*VIDEO_DMA_W+880]==VIDEO_BLANK);
 uint16_t *lcd=malloc(480*360*2);assert(lcd);
 for(int i=0;i<480*320;i++)lcd[i]=0x00f8; // raw byte-swapped red
 video_convert(frame,lcd,480,320,NULL,true,false,0,0,NULL);
 assert(frame[12*320]==0&&frame[13*320]==224&&frame[225*320+319]==224&&frame[226*320]==0);
 video_convert(frame,lcd,480,320,NULL,true,true,240,160,NULL);
 assert(frame[119*320+160]==0&&frame[121*320+161]==255);
 video_convert(frame,lcd,480,320,NULL,true,true,479,319,NULL);
 assert(frame[225*320+319]==0); // clipped hotspot at right edge
 uint32_t rgb[256]={0};rgb[1]=255;rgb[2]=0xff00;rgb[3]=0xff0000;
 uint8_t *nes=malloc(256*240);assert(nes);memset(nes,2,256*240);
 video_convert(frame,nes,256,240,rgb,false,false,0,0,NULL);
 assert(frame[31]==0&&frame[32]==28&&frame[287]==28&&frame[288]==0);
 // Logical 256x240 NES has square pixels at this seam; OS sends its LCD
 // framebuffer and preserves the same final UI aspect. Doom is full 4:3.
 memset(nes,3,256*240);video_convert(frame,nes,256,240,rgb,false,true,255,239,NULL);
 assert(frame[239*320+286]==3&&frame[239*320+287]==0);
 // Alignment acts on owned image pixels, never the NTSC sync/burst region.
 freslest_tv_config_t cfg={16,-12,90};memset(lcd,0xff,480*320*2);
 video_convert(frame,lcd,480,320,NULL,false,false,0,0,&cfg);
 assert(frame[11*320+32]==0&&frame[12*320+31]==0);
 int first=-1,last=-1,count=0;
 for(int i=0;i<320*240;i++)if(frame[i]){if(first<0)first=i;last=i;count++;}
 assert(first==12*320+32&&last==202*320+319&&count==288*191);
 // Extreme settings clamp inside the canvas; cursor hotspot follows the image.
 cfg=(freslest_tv_config_t){127,-128,80};
 video_convert(frame,lcd,480,320,NULL,false,true,240,160,&cfg);
 assert(frame[96*320+192]==0&&frame[98*320+193]==255);
 // True 4:3 UI: uniform 2/3 scale, all 320x240 pixels active.
 for(int i=0;i<480*360;i++)lcd[i]=0x00f8;
 video_convert(frame,lcd,480,360,NULL,true,false,0,0,NULL);
 for(int i=0;i<320*240;i++)assert(frame[i]==224);
 memset(lcd,0,480*360*2);
 for(int y=100;y<250;y++)for(int x=150;x<300;x++)lcd[y*480+x]=0xffff;
 video_convert(frame,lcd,480,360,NULL,false,false,0,0,NULL);
 count=0;for(int y=0;y<240;y++)for(int x=0;x<320;x++){
   assert((frame[y*320+x]!=0)==(x>=100&&x<200&&y>=67&&y<167));if(frame[y*320+x])count++;
 }assert(count==10000); // 150x150 source square remains 100x100, no stretch.
 memset(lcd,0xff,480*360*2);
 video_convert(frame,lcd,480,360,NULL,false,true,479,359,NULL);assert(frame[239*320+319]==0);
 uint8_t *before=malloc(n);assert(before);memcpy(before,wave,n);
 video_encode_wave(wave,frame,palette);
 for(int y=0;y<VIDEO_DMA_H;y++)for(int x=0;x<VIDEO_DMA_W;x++)
  if(y<VIDEO_IMAGE_TOP||y>=VIDEO_IMAGE_TOP+VIDEO_H||x<VIDEO_IMAGE_START||x>=VIDEO_IMAGE_START+VIDEO_IMAGE_SAMPLES)
   assert(wave[y*VIDEO_DMA_W+x]==before[y*VIDEO_DMA_W+x]);
 free(before);
 video_slots_t slots;video_slots_init(&slots);
 int a=video_slots_acquire(&slots);assert(a>=0&&video_slots_acquire(&slots)==-1);video_slots_publish(&slots,a);
 assert(video_slots_consume(&slots)==a);int b=video_slots_acquire(&slots);assert(b!=a);
 video_slots_publish(&slots,b);int c=video_slots_acquire(&slots);assert(c!=a&&c!=b);
 video_slots_publish(&slots,c);assert(video_slots_consume(&slots)==c); // drop stale ready frame
 for(int i=0;i<10000;i++){int writing=video_slots_acquire(&slots);assert(writing>=0&&writing!=slots.reading);video_slots_publish(&slots,writing);if(i%3)assert(video_slots_consume(&slots)==writing);}
 assert(freslest_cursor_pixel(-1,0)==0&&freslest_cursor_pixel(13,18)==0&&freslest_cursor_pixel(0,19)==0);
 free(nes);free(lcd);free(frame);free(aligned);
 puts("PASS: exact composite buffer bounds, 910x262 timing budget, alternating burst, black/white levels, full waveform, RGB565 byte order, indexed conversion, 4:3 uniform 480x360-to-320x240 scale and square/cursor bounds, TV calibration and invariant sync/burst, and triple-buffer ownership");
}
