// Waveform constants and YUV phase encoding adapted from the supplied
// Freslest_Composite_Teknik_Kaynak/components/platform/composite_out.c.
#include "video_pixels.h"
#include "freslest_cursor.h"
#include "av_signal.h"
#include <string.h>
static uint8_t rgb332(unsigned r,unsigned g,unsigned b){return (r&224)|((g&224)>>3)|(b>>6);}
void video_phase_palette(uint8_t palette[256][4]){av_signal_palette(palette,0,100);}
void video_color_palette(uint8_t palette[256][4],const freslest_tv_color_t *cfg){av_signal_palette(palette,cfg?cfg->hue:0,cfg?cfg->saturation:100);}
// Legacy waveform routines remain only for reference/regression tests. The
// production backend uses av_signal.c and the raw freslest_av transport.
void video_burst_wave(uint8_t *wave,bool enabled) {
 static const int burst[4]={36,0,-36,0};
 for(int y=0;y<VIDEO_DMA_H;y++){
  if(y>=248&&y<251)continue;
  for(int i=75;i<115;i++)wave[y*VIDEO_DMA_W+i]=VIDEO_BLANK+(enabled?burst[(i+y*VIDEO_LINE_SAMPLES)&3]:0);
 }
}
void video_prime_wave(uint8_t *wave) {
 for(int y=0;y<VIDEO_DMA_H;y++) {
  uint8_t *row=wave+y*VIDEO_DMA_W;
  memset(row,VIDEO_BLANK,VIDEO_DMA_W);
  if(y>=248&&y<251){memset(row,0,VIDEO_DMA_W-67);continue;}
  memset(row,0,67);
  if(y>=VIDEO_IMAGE_TOP&&y<VIDEO_IMAGE_TOP+VIDEO_H)
   memset(row+VIDEO_IMAGE_START,VIDEO_BLACK,VIDEO_IMAGE_SAMPLES);
 }
 video_burst_wave(wave,true);
}
static void encode_sharp(uint8_t *wave,const uint8_t *pixels,const uint8_t palette[256][4]) {
 // A four-sample moving window has a notch at the colour carrier. Apply the
 // same window to all four phase lanes, never to the modulated waveform:
 // flat-field colour survives, while fine grey edges create less false colour.
 // Rolling sums need one new palette lookup per output sample.
 uint16_t sx[VIDEO_IMAGE_SAMPLES];
 for(int i=0;i<VIDEO_IMAGE_SAMPLES;i++)sx[i]=i*VIDEO_W/VIDEO_IMAGE_SAMPLES;
 for(int y=0;y<VIDEO_H;y++) {
  const uint8_t *src=pixels+y*VIDEO_W;
  uint32_t *out=(uint32_t*)(wave+(y+VIDEO_IMAGE_TOP)*VIDEO_DMA_W+VIDEO_IMAGE_START);
  // Phase follows the physical 910-sample line, not the 900-byte DMA row.
  // Matching the burst's alternating phase lets a TV's line comb separate
  // chroma from repeated fine luma detail instead of colouring white text.
  const unsigned line_phase=((y+VIDEO_IMAGE_TOP)*VIDEO_LINE_SAMPLES+VIDEO_IMAGE_START)&3;
  uint32_t ring[4],lo=0,hi=0;
  for(int j=0;j<4;j++){
   int x=j?j-1:0;memcpy(&ring[j],palette[src[sx[x]]],4);
   lo+=ring[j]&0x00ff00ffu;hi+=(ring[j]>>8)&0x00ff00ffu;
  }
  for(int i=0;i<VIDEO_IMAGE_SAMPLES;i+=4){
   uint32_t packed=0;
   for(unsigned k=0;k<4;k++){
    int x=i+k;unsigned p=(k+line_phase)&3;uint32_t sum=p&1?hi:lo;
    unsigned value=(((sum>>((p&2)*8))&0xffffu)+2)/4;
    packed|=value<<(k*8);
    uint32_t old=ring[k],next;int nx=x+3;if(nx>=VIDEO_IMAGE_SAMPLES)nx=VIDEO_IMAGE_SAMPLES-1;
    memcpy(&next,palette[src[sx[nx]]],4);ring[k]=next;
    lo=lo-(old&0x00ff00ffu)+(next&0x00ff00ffu);
    hi=hi-((old>>8)&0x00ff00ffu)+((next>>8)&0x00ff00ffu);
   }
   *out++=packed;
  }
 }
}
static void encode_clean(uint8_t *wave,const uint8_t *pixels,const uint8_t palette[256][4]) {
 // Three cascaded four-sample boxes: [1,3,6,10,12,12,10,6,3,1]/64.
 // The wider carrier rejection covers sidebands as well as the exact fsc
 // notch. Filtering unmodulated phase lanes preserves a flat colour vector.
 // Packed 16-bit sums avoid per-tap multiplies and any per-frame allocation.
 typedef struct {uint32_t lo,hi;} pair_t;
 uint16_t sx[VIDEO_IMAGE_SAMPLES];
 for(int i=0;i<VIDEO_IMAGE_SAMPLES;i++)sx[i]=i*VIDEO_W/VIDEO_IMAGE_SAMPLES;
 for(int y=0;y<VIDEO_H;y++){
  const uint8_t *src=pixels+y*VIDEO_W;
  uint32_t *out=(uint32_t*)(wave+(y+VIDEO_IMAGE_TOP)*VIDEO_DMA_W+VIDEO_IMAGE_START);
  const unsigned line_phase=((y+VIDEO_IMAGE_TOP)*VIDEO_LINE_SAMPLES+VIDEO_IMAGE_START)&3;
  uint32_t first,raw[4];memcpy(&first,palette[src[0]],4);
  pair_t a={(first&0x00ff00ffu)*4,((first>>8)&0x00ff00ffu)*4};
  pair_t b={a.lo*4,a.hi*4},c={b.lo*4,b.hi*4},second[4],third[4];
  for(int j=0;j<4;j++){raw[j]=first;second[j]=a;third[j]=b;}
  uint32_t packed=0;
  for(int t=-3;t<VIDEO_IMAGE_SAMPLES+5;t++){
   unsigned slot=(unsigned)t&3;int at=t<0?0:t>=VIDEO_IMAGE_SAMPLES?VIDEO_IMAGE_SAMPLES-1:t;
   uint32_t next,old=raw[slot];memcpy(&next,palette[src[sx[at]]],4);raw[slot]=next;
   a.lo=a.lo-(old&0x00ff00ffu)+(next&0x00ff00ffu);
   a.hi=a.hi-((old>>8)&0x00ff00ffu)+((next>>8)&0x00ff00ffu);
   b.lo=b.lo-second[slot].lo+a.lo;b.hi=b.hi-second[slot].hi+a.hi;second[slot]=a;
   c.lo=c.lo-third[slot].lo+b.lo;c.hi=c.hi-third[slot].hi+b.hi;third[slot]=b;
   int x=t-5;if(x<0)continue;
   unsigned phase=(x+line_phase)&3;uint32_t sum=phase&1?c.hi:c.lo;
   unsigned value=(((sum>>((phase&2)*8))&0xffffu)+32)/64;
   packed|=value<<((x&3)*8);
   if((x&3)==3){*out++=packed;packed=0;}
  }
 }
}
void video_encode_wave_detail(uint8_t *wave,const uint8_t *pixels,const uint8_t palette[256][4],unsigned detail){
 if(detail==FRESLEST_TV_SHARP)encode_sharp(wave,pixels,palette);
 else encode_clean(wave,pixels,palette);
}
void video_encode_wave(uint8_t *wave,const uint8_t *pixels,const uint8_t palette[256][4]){
 video_encode_wave_detail(wave,pixels,palette,FRESLEST_TV_CLEAN);
}
void video_convert(uint8_t *dest,const void *source,int w,int h,const uint32_t *pal,bool swapped,bool cursor,int cx,int cy,const freslest_tv_config_t *config) {
 if(!dest||!source||w<1||h<1||w>480||h>360)return;
 int out_w=VIDEO_W,out_h=VIDEO_H;
 if(w*VIDEO_H>h*VIDEO_W)out_h=h*VIDEO_W/w;else out_w=w*VIDEO_H/h;
 unsigned scale=config?config->scale:100;
 if(scale<80||scale>100)scale=100;
 int max_x=(VIDEO_W-VIDEO_W*(int)scale/100)/2,max_y=(VIDEO_H-VIDEO_H*(int)scale/100)/2;
 int dx=config?config->x:0,dy=config?config->y:0;
 if(dx < -max_x)dx=-max_x;
 if(dx>max_x)dx=max_x;
 if(dy < -max_y)dy=-max_y;
 if(dy>max_y)dy=max_y;
 out_w=out_w*(int)scale/100;out_h=out_h*(int)scale/100;
 int left=(VIDEO_W-out_w)/2+dx,top=(VIDEO_H-out_h)/2+dy;
 memset(dest,0,VIDEO_W*VIDEO_H);
 uint16_t sx[VIDEO_W];uint8_t indexed[256];
 for(int x=0;x<out_w;x++)sx[x]=x*w/out_w;
 if(pal)for(int i=0;i<256;i++)indexed[i]=rgb332(pal[i]&255,(pal[i]>>8)&255,(pal[i]>>16)&255);
 for(int y=0;y<out_h;y++) {
  int offset=(y*h/out_h)*w;uint8_t *out=dest+(y+top)*VIDEO_W+left;
  if(pal){const uint8_t *row=(const uint8_t*)source+offset;for(int x=0;x<out_w;x++)out[x]=indexed[row[sx[x]]];}
  else {const uint16_t *row=(const uint16_t*)source+offset;
   for(int x=0;x<out_w;x++){uint16_t v=row[sx[x]];if(swapped)v=(v>>8)|(v<<8);
    out[x]=(v>>8&224)|(v>>6&28)|(v>>3&3);}
  }
 }
 if(cursor) {
  cx=left+cx*out_w/w;cy=top+cy*out_h/h;
  for(int y=0;y<FRESLEST_CURSOR_H&&cy+y<VIDEO_H;y++)
   for(int x=0;x<FRESLEST_CURSOR_W&&cx+x<VIDEO_W;x++) {
    unsigned p=freslest_cursor_pixel(x,y);
    if(p&&cx+x>=0&&cy+y>=0)dest[(cy+y)*VIDEO_W+cx+x]=p==1?0:p==2?255:31;
   }
 }
}
