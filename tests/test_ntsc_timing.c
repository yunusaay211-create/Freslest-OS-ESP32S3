// Receiver-side model: NTSC's 227.5 carrier cycles per line are independent
// of the driver's packed palette/FIR implementation. No TV measurement.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "video_pixels.h"
static const int cosine[4]={1,0,-1,0},sine[4]={0,1,0,-1};
int main(void){
 assert(VIDEO_LINE_SAMPLES==4*227+2);
 assert((VIDEO_FIELD_LINES*VIDEO_LINE_SAMPLES)%4==0);
 assert(VIDEO_IMAGE_START+VIDEO_IMAGE_SAMPLES<=VIDEO_DMA_W);
 const double sample=315000000.0/22.0;
 assert(fabs(VIDEO_SAMPLES-sample)<1);
 printf("NTSC model: %.6f MHz carrier, %.6f kHz horizontal, %.6f Hz progressive refresh\n",
        sample/4e6,sample/910e3,sample/(910*262));
 const size_t bytes=VIDEO_DMA_W*VIDEO_DMA_H;
 uint8_t *wave=malloc(bytes+32),*pixels=malloc(VIDEO_W*VIDEO_H);assert(wave&&pixels);
 memset(wave,0xa5,bytes+32);video_prime_wave(wave);
 uint8_t palette[256][4];video_phase_palette(palette);
 // A receiver locks to a continuous 3.579545 MHz oscillator. Adjacent
 // bursts must be opposite at the same horizontal coordinate, including
 // lines before and after the image. The 262-line restart must be coherent.
 for(int y=0;y<VIDEO_DMA_H;y++){
  if(y>=248&&y<251)continue;
  for(int x=75;x<115;x++){
   unsigned phase=(x+2*y)%4;
   assert((int)wave[y*VIDEO_DMA_W+x]-VIDEO_BLANK==36*cosine[phase]);
  }
 }
 const unsigned colours[]={224,28,3,252,31,227,0x6d,255,0};
 double max_error=0;
 for(unsigned n=0;n<sizeof(colours)/sizeof(*colours);n++){
  unsigned index=colours[n];memset(pixels,index,VIDEO_W*VIDEO_H);
  video_encode_wave(wave,pixels,palette);
  double expected_p=(palette[index][0]-palette[index][2])*.5;
  double expected_q=(palette[index][1]-palette[index][3])*.5;
  // A one-line comb's difference extracts colour. Lock its demodulator
  // to the continuous oscillator, not to a palette lane chosen by the driver.
  for(int y=VIDEO_IMAGE_TOP;y<VIDEO_IMAGE_TOP+VIDEO_H-1;y++){
   double p=0,q=0;
   for(int x=0;x<VIDEO_IMAGE_SAMPLES;x++){
    int at=VIDEO_IMAGE_START+x;
    double chroma=(wave[y*VIDEO_DMA_W+at]-wave[(y+1)*VIDEO_DMA_W+at])*.5;
    unsigned phase=(at+2*y)%4;
    p+=chroma*cosine[phase];q+=chroma*sine[phase];
   }
   p*=2.0/VIDEO_IMAGE_SAMPLES;q*=2.0/VIDEO_IMAGE_SAMPLES;
   double err=hypot(p-expected_p,q-expected_q);if(err>max_error)max_error=err;
   assert(err<.01);
  }
 }
 // Fine monochrome texture repeated on adjacent rows belongs to luma.
 // It must cancel entirely in the line-comb chroma output. The comb's
 // average must retain the actual filtered text detail, not blank the UI.
 unsigned state=71;
 for(int x=0;x<VIDEO_W;x++){state=state*1664525+1013904223;pixels[x]=state&0x80000000?255:0;}
 for(int y=1;y<VIDEO_H;y++)memcpy(pixels+y*VIDEO_W,pixels,VIDEO_W);
 video_encode_wave(wave,pixels,palette);
 int min=255,max=0;
 for(int y=VIDEO_IMAGE_TOP;y<VIDEO_IMAGE_TOP+VIDEO_H-1;y++)
  for(int x=VIDEO_IMAGE_START;x<VIDEO_IMAGE_START+VIDEO_IMAGE_SAMPLES;x++){
   int a=wave[y*VIDEO_DMA_W+x],b=wave[(y+1)*VIDEO_DMA_W+x];
   assert(a==b);if(a<min)min=a;if(a>max)max=a;
  }
 assert(max-min>100);
 for(int i=0;i<32;i++)assert(wave[bytes+i]==0xa5);
 printf("PASS: 227.5-cycle NTSC line/field phase continuity, alternating burst, all-line flat-colour comb recovery (max error %.6f DAC codes), zero model cross-colour on repeated monochrome detail, retained luma contrast and DMA guards\n",max_error);
 free(pixels);free(wave);
}
