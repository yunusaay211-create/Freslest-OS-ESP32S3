// Band-energy test with the actual OS GLCD glyphs and RGB565 text colours.
// The receiver model needs no line comb. It is not a Samsung simulation.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "video_pixels.h"
#include "av_signal.h"
#define PROGMEM
#include "glcdfont.h"
static uint16_t rgb[480*360];
static uint16_t swap(uint16_t v){return (v>>8)|(v<<8);}
static void text(int x,int y,const char *s,int scale,uint16_t color){
 for(;*s;s++,x+=6*scale)for(int col=0;col<5;col++)for(int row=0;row<8;row++)
  if(font[(unsigned char)*s*5+col]&(1u<<row))
   for(int dy=0;dy<scale;dy++)for(int dx=0;dx<scale;dx++){
    int atx=x+col*scale+dx,aty=y+row*scale+dy;
    if(atx>=0&&atx<480&&aty>=0&&aty<360)rgb[aty*480+atx]=swap(color);
   }
}
static double band_energy(const uint8_t *wave,double low,double high){
 double total=0;const double fs=315000000.0/22.0;
 int first=(int)ceil(low*AV_ACTIVE_SAMPLES/fs),last=(int)floor(high*AV_ACTIVE_SAMPLES/fs);
 for(int y=0;y<VIDEO_H;y++){
  const uint8_t *s=wave+(y+AV_ACTIVE_TOP)*AV_LINE+AV_ACTIVE_START;
  for(int bin=first;bin<=last;bin++){
   double angle=2*M_PI*bin/AV_ACTIVE_SAMPLES,c=cos(angle),sn=sin(angle),re=0,im=0,cr=1,si=0;
   for(int x=0;x<AV_ACTIVE_SAMPLES;x++){
    double v=(int)s[x]-VIDEO_BLACK;re+=v*cr;im+=v*si;
    double next=cr*c-si*sn;si=si*c+cr*sn;cr=next;
   }
   total+=re*re+im*im;
  }
 }
 return total;
}
int main(void){
 size_t n=AV_LINE*AV_LINES;
 uint8_t *clean=malloc(n+32),*sharp=malloc(n+32),*pixels=malloc(VIDEO_W*VIDEO_H),*before=malloc(n);
 assert(clean&&sharp&&pixels&&before);memset(clean,0xa5,n+32);memset(sharp,0xa5,n+32);
 av_renderer_t *renderer=malloc(sizeof(*renderer));assert(renderer);av_renderer_init(renderer);av_signal_prime(clean,true);av_signal_prime(sharp,true);uint8_t palette[256][4];video_phase_palette(palette);
 const char *items[]={"Apps","Settings","System Info","Test","Game","Freslest Update"};
 text(10,4,"FRESLEST",2,0xffff);
 for(int i=0;i<6;i++)text(64,63+i*39,items[i],2,0xa514);
 text(10,342,"UP/DOWN Move OK Enter",1,0xffff);
 video_convert(pixels,rgb,480,360,NULL,true,false,0,0,NULL);
 // There is no intended colour in the test UI before it reaches the encoder.
 for(int i=0;i<VIDEO_W*VIDEO_H;i++)for(int k=1;k<4;k++)assert(palette[pixels[i]][k]==palette[pixels[i]][0]);
 av_signal_encode(renderer,clean,pixels,palette,0);
 av_signal_encode(renderer,sharp,pixels,palette,1);
 double a=band_energy(clean,3579545.454545-1200000,3579545.454545+1200000);
 double b=band_energy(sharp,3579545.454545-1200000,3579545.454545+1200000);
 assert(b>1000&&a/b<.3);
 double luma_clean=band_energy(clean,150000,1200000),luma_sharp=band_energy(sharp,150000,1200000);
 assert(luma_clean/luma_sharp>.5); // retain readable low-frequency glyph structure
 printf("PASS: new engine with actual GLCD menu/header/footer glyphs and RGB565 grey stay neutral before encoding; no-comb chroma-band energy Clean/Sharp %.6f, low-band luma retained %.6f (software model only)\n",a/b,luma_clean/luma_sharp);
 // The two filters have exactly the same flat-field levels and chroma vectors.
 const unsigned colours[]={224,28,3,252,31,227,0,255,0x6d};
 for(unsigned i=0;i<sizeof(colours)/sizeof(*colours);i++){
  memset(pixels,colours[i],VIDEO_W*VIDEO_H);
  av_signal_encode(renderer,clean,pixels,palette,0);
  av_signal_encode(renderer,sharp,pixels,palette,1);assert(!memcmp(clean,sharp,n));
 }
 // Disabling burst affects only burst slots, never sync, active image or guards.
 memcpy(before,clean,n);av_signal_burst(clean,false);
 for(int y=0;y<AV_LINES;y++)for(int x=0;x<AV_LINE;x++){
  int at=y*AV_LINE+x;
  if(y>=9&&x>=75&&x<115)assert(clean[at]==VIDEO_BLANK);
  else assert(clean[at]==before[at]);
 }
 av_signal_burst(clean,true);assert(!memcmp(clean,before,n));
 for(int i=0;i<32;i++)assert(clean[n+i]==0xa5&&sharp[n+i]==0xa5);
 puts("PASS: Clean/Sharp flat-colour equivalence, reversible no-burst monochrome without sync/image changes, and DMA guards");
 free(renderer);free(before);free(pixels);free(sharp);free(clean);
}
