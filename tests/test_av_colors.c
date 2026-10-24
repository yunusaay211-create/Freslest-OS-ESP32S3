#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "video_pixels.h"
static double angle_error(double a,double b){double d=fmod(a-b+540.0,360.0)-180.0;return fabs(d);}
int main(void){
 uint8_t palette[256][4];video_phase_palette(palette);
 // Decode the primary/secondary chroma vectors relative to the actual burst.
 const unsigned colors[]={224,28,3,252,31,227};
 for(unsigned j=0;j<6;j++){
  unsigned i=colors[j];double r=((i>>5)&7)*255.0/7,g=((i>>2)&7)*255.0/7,b=(i&3)*85.0;
  double U=-.147407*r-.289391*g+.436798*b,V=.614777*r-.514799*g-.099978*b;
  double P=(palette[i][0]-palette[i][2])*.5,Q=(palette[i][1]-palette[i][3])*.5;
  assert(angle_error(atan2(Q,P)*180/M_PI,atan2(V,-U)*180/M_PI)<2.0);
 }
 // All eight grey levels stay neutral for every hue, including maximum colour.
 for(int h=-180;h<=180;h+=5){
  freslest_tv_color_t c={h,160,0};video_color_palette(palette,&c);
  for(unsigned n=0;n<8;n++){unsigned i=(n<<5)|(n<<2)|(n>>1);
   for(unsigned k=1;k<4;k++)assert(palette[i][k]==palette[i][0]);}
 }
 freslest_tv_color_t c={90,100,0};video_color_palette(palette,&c);
 double p=(palette[224][0]-palette[224][2])*.5,q=(palette[224][1]-palette[224][3])*.5;
 video_phase_palette(palette);double p0=(palette[224][0]-palette[224][2])*.5,q0=(palette[224][1]-palette[224][3])*.5;
 assert(angle_error(atan2(q,p)*180/M_PI,atan2(q0,p0)*180/M_PI-90)<2.0);
 c=(freslest_tv_color_t){0,0,0};video_color_palette(palette,&c);
 for(unsigned i=0;i<256;i++)for(unsigned k=1;k<4;k++)assert(palette[i][k]==palette[i][0]);
 // Every representable RGB565 grey enters the neutral palette diagonal.
 uint8_t *dest=malloc(VIDEO_W*VIDEO_H),*source=malloc(VIDEO_W*VIDEO_H);
 uint16_t *rgb=malloc(VIDEO_W*VIDEO_H*2);assert(dest&&source&&rgb);
 video_phase_palette(palette);
 for(unsigned n=0;n<32;n++){
  uint16_t value=(n<<11)|(((n<<1)|(n>>4))<<5)|n;
  for(int i=0;i<VIDEO_W*VIDEO_H;i++)rgb[i]=value;
  video_convert(dest,rgb,VIDEO_W,VIDEO_H,NULL,false,false,0,0,NULL);
  unsigned index=dest[0];for(unsigned k=1;k<4;k++)assert(palette[index][k]==palette[index][0]);
 }
 uint8_t *wave=malloc(VIDEO_DMA_W*VIDEO_DMA_H+64);assert(wave);memset(wave,0xa5,VIDEO_DMA_W*VIDEO_DMA_H+64);video_prime_wave(wave);
 uint8_t *before=malloc(VIDEO_DMA_W*VIDEO_DMA_H);assert(before);memcpy(before,wave,VIDEO_DMA_W*VIDEO_DMA_H);
 unsigned state=17;for(int i=0;i<VIDEO_W*VIDEO_H;i++){state=state*1664525+1013904223;source[i]=state>>24;}
 // Compare both output modes against independent scalar FIR coefficients.
 for(unsigned detail=0;detail<=1;detail++){
  video_encode_wave_detail(wave,source,palette,detail);
  const unsigned clean[]={1,3,6,10,12,12,10,6,3,1},sharp[]={1,1,1,1};
  int left=detail?-1:-4,right=detail?2:5;unsigned divisor=detail?4:64;
  const unsigned *weights=detail?sharp:clean;
  for(int y=0;y<VIDEO_H;y++)for(int x=0;x<VIDEO_IMAGE_SAMPLES;x++){
   unsigned sum=0;for(int j=left;j<=right;j++){
    int xx=x+j;if(xx<0)xx=0;if(xx>=VIDEO_IMAGE_SAMPLES)xx=VIDEO_IMAGE_SAMPLES-1;
    unsigned phase=(x+VIDEO_IMAGE_START+(y+VIDEO_IMAGE_TOP)*VIDEO_LINE_SAMPLES)&3;
    sum+=weights[j-left]*palette[source[y*VIDEO_W+xx*VIDEO_W/VIDEO_IMAGE_SAMPLES]][phase];
   }
   assert(wave[(y+VIDEO_IMAGE_TOP)*VIDEO_DMA_W+VIDEO_IMAGE_START+x]==(sum+divisor/2)/divisor);
  }
 }
 for(int y=0;y<VIDEO_DMA_H;y++)for(int x=0;x<VIDEO_DMA_W;x++)
  if(y<VIDEO_IMAGE_TOP||y>=VIDEO_IMAGE_TOP+VIDEO_H||x<VIDEO_IMAGE_START||x>=VIDEO_IMAGE_START+VIDEO_IMAGE_SAMPLES)
   assert(wave[y*VIDEO_DMA_W+x]==before[y*VIDEO_DMA_W+x]);
 // A pure grey field contains no chroma modulation at all.
 memset(source,0x6d,VIDEO_W*VIDEO_H);video_encode_wave(wave,source,palette);
 for(int x=0;x<VIDEO_IMAGE_SAMPLES;x++)assert(wave[VIDEO_IMAGE_TOP*VIDEO_DMA_W+VIDEO_IMAGE_START+x]==palette[0x6d][0]);
 // Fine black/white edges: measure energy at the colour carrier, before/after.
 state=71;for(int x=0;x<VIDEO_W;x++){state=state*1664525+1013904223;source[x]=state&0x80000000?255:0;}
 for(int y=1;y<VIDEO_H;y++)memcpy(source+y*VIDEO_W,source,VIDEO_W);
 video_encode_wave(wave,source,palette);double oldI=0,oldQ=0,newI=0,newQ=0;
 for(int x=0;x<VIDEO_IMAGE_SAMPLES;x++){
  double a=palette[source[x*VIDEO_W/VIDEO_IMAGE_SAMPLES]][x&3],b=wave[VIDEO_IMAGE_TOP*VIDEO_DMA_W+VIDEO_IMAGE_START+x];
  if((x&3)==0){oldI+=a;newI+=b;}if((x&3)==2){oldI-=a;newI-=b;}
  if((x&3)==1){oldQ+=a;newQ+=b;}if((x&3)==3){oldQ-=a;newQ-=b;}
 }
 double ratio=(newI*newI+newQ*newQ)/(oldI*oldI+oldQ*oldQ);assert(ratio<.05);
 for(int i=0;i<64;i++)assert(wave[VIDEO_DMA_W*VIDEO_DMA_H+i]==0xa5);
 printf("PASS: AV chroma vectors/hue rotation, 8 neutral greys at every hue, all RGB565 greys, monochrome color=0, Clean/Sharp scalar-vs-rolling FIR, exact sync/burst and guards; model carrier-energy ratio %.6f\n",ratio);
 free(before);free(wave);free(rgb);free(source);free(dest);
}
