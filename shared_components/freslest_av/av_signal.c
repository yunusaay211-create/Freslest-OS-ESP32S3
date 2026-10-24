// SPDX-License-Identifier: MIT
#include "av_signal.h"
#include <string.h>
#include <math.h>
static uint8_t dac(float value){int v=(int)lroundf(value);return v<0?0:v>255?255:v;}
void av_signal_palette(uint8_t palette[256][4],int hue,unsigned saturation){
 if(hue < -180||hue>180)hue=0;
 if(saturation>160)saturation=100;
 float angle=hue*(3.14159265358979323846f/180),cs=cosf(angle),sn=sinf(angle),gain=.5f*saturation/100;
 for(unsigned i=0;i<256;i++){
  unsigned red=i>>5,green=(i>>2)&7,blue=i&3;
  bool grey=red==green&&blue==red/2;
  float r=red*255.0f/7,g=green*255.0f/7,b=grey?r:blue*85.0f;
  float y=.299f*r+.587f*g+.114f*b;
  float u=grey?0:-.147407f*r-.289391f*g+.436798f*b,v=grey?0:.614777f*r-.514799f*g-.099978f*b;
  float p=-gain*(u*cs-v*sn),q=gain*(u*sn+v*cs),l=AV_BLACK+y*(AV_WHITE-AV_BLACK)/255;
  float peak=fmaxf(fabsf(p),fabsf(q)),room=fminf(l,AV_WHITE-l);
  if(peak>room&&peak>0){p*=room/peak;q*=room/peak;}
  palette[i][0]=dac(l+p);palette[i][1]=dac(l+q);palette[i][2]=dac(l-p);palette[i][3]=dac(l-q);
 }
}
void av_signal_burst(uint8_t *wave,bool enabled){
 static const int8_t carrier[4]={36,0,-36,0};
 for(unsigned y=9;y<AV_LINES;y++)for(unsigned x=AV_BURST_START;x<AV_BURST_END;x++)
  wave[y*AV_LINE+x]=AV_BLANK+(enabled?carrier[(y*AV_LINE+x)&3]:0);
}
void av_signal_prime(uint8_t *wave,bool burst){
 memset(wave,AV_BLANK,AV_FIELD_BYTES);
 for(unsigned y=0;y<AV_LINES;y++){
  uint8_t *line=wave+y*AV_LINE;
  if(y<9){
   // Three lines pre-equalization, three serrated broad-sync lines,
   // three post-equalization lines. This is a 262-line progressive field.
   unsigned low=y>=3&&y<6?AV_HALF_LINE-AV_HSYNC:AV_EQUALIZE;
   memset(line,AV_SYNC,low);memset(line+AV_HALF_LINE,AV_SYNC,low);
  }else{
   memset(line,AV_SYNC,AV_HSYNC);
   if(y>=AV_ACTIVE_TOP&&y<AV_ACTIVE_TOP+AV_HEIGHT)
    memset(line+AV_ACTIVE_START,AV_BLACK,AV_ACTIVE_SAMPLES);
  }
 }
 av_signal_burst(wave,burst);
}
void av_renderer_init(av_renderer_t *r){
 for(unsigned x=0;x<AV_ACTIVE_SAMPLES;x++){
  unsigned fixed=x*(AV_WIDTH-1)*256u/(AV_ACTIVE_SAMPLES-1);
  r->source[x]=fixed>>8;r->fraction[x]=fixed&255;
 }
}
void av_signal_encode(av_renderer_t *r,uint8_t *wave,const uint8_t *canvas,const uint8_t palette[256][4],unsigned detail){
 if(detail!=1)for(unsigned i=0;i<256;i++)r->colors[i]=(uint32_t)palette[i][0]|((uint32_t)palette[i][1]<<8)|((uint32_t)palette[i][2]<<16)|((uint32_t)palette[i][3]<<24);
 for(unsigned y=0;y<AV_HEIGHT;y++){
  const uint8_t *src=canvas+y*AV_WIDTH;
  unsigned physical=y+AV_ACTIVE_TOP;
  uint8_t *out=wave+physical*AV_LINE+AV_ACTIVE_START;
  if(detail==1){
   for(unsigned x=0;x<AV_ACTIVE_SAMPLES;x++){
    unsigned at=r->source[x],next=at+1<AV_WIDTH?at+1:at,f=r->fraction[x],p=(physical*AV_LINE+AV_ACTIVE_START+x)&3;
    out[x]=(palette[src[at]][p]*(256-f)+palette[src[next]][p]*f+128)>>8;
   }
   continue;
  }
  for(unsigned x=0;x<AV_ACTIVE_SAMPLES;x++){
   unsigned at=r->source[x],next=at+1<AV_WIDTH?at+1:at,f=r->fraction[x];
   uint32_t a=r->colors[src[at]],b=r->colors[src[next]],mask=0x00ff00ffu;
   // Two independent 16-bit lanes: each weighted sum is at most 65408.
   uint32_t lo=((a&mask)*(256-f)+(b&mask)*f+0x00800080u)>>8;
   uint32_t hi=(((a>>8)&mask)*(256-f)+((b>>8)&mask)*f+0x00800080u)>>8;
   r->phases[x]=(lo&mask)|((hi&mask)<<8);
  }
  uint32_t lo=0,hi=0;
  for(int k=-1;k<=2;k++){uint32_t v=r->phases[k<0?0:k];lo+=v&0x00ff00ffu;hi+=(v>>8)&0x00ff00ffu;}
  for(unsigned x=0;x<AV_ACTIVE_SAMPLES;x++){
   unsigned phase=(physical*AV_LINE+AV_ACTIVE_START+x)&3;
   uint32_t sum=phase&1?hi:lo;
   unsigned value=(((sum>>((phase&2)*8))&0xffffu)+2)/4;
   // Odd 910-byte scanlines are not 32-bit aligned. Byte stores are
   // intentional; casting out to uint32_t would fault on ESP32-S3.
   out[x]=value;
   unsigned old=x?x-1:0,next=x+3<AV_ACTIVE_SAMPLES?x+3:AV_ACTIVE_SAMPLES-1;
   uint32_t a=r->phases[old],b=r->phases[next];
   lo=lo-(a&0x00ff00ffu)+(b&0x00ff00ffu);hi=hi-((a>>8)&0x00ff00ffu)+((b>>8)&0x00ff00ffu);
  }
 }
}
static void rect(uint8_t *c,int x,int y,int w,int h,uint8_t color){
 for(int j=y;j<y+h&&j<AV_HEIGHT;j++)for(int i=x;i<x+w&&i<AV_WIDTH;i++)
  if(i>=0&&j>=0)c[j*AV_WIDTH+i]=color;
}
static void label(uint8_t *c,int x,int y,const char *s){
 // Small original 5x7 glyph set used only by the transport diagnostic.
 static const uint8_t letters[26][5]={
  {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
  {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
  {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
  {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
  {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
  {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
  {7,8,112,8,7},{97,81,73,69,67}
 };
 for(;*s;s++,x+=12){
  if(*s<'A'||*s>'Z')continue;
  for(int i=0;i<5;i++)for(int j=0;j<7;j++)if(letters[*s-'A'][i]&(1u<<j))rect(c,x+i*2,y+j*2,2,2,255);
 }
}
void av_signal_pattern(uint8_t *c,unsigned pattern){
 pattern%=4;memset(c,pattern==3?255:0,AV_WIDTH*AV_HEIGHT);
 if(pattern==3)return;
 if(pattern==1){
  const uint8_t bars[]={182,180,22,20,162,160,2,0};
  for(int i=0;i<8;i++)rect(c,i*40,48,40,136,bars[i]);
  label(c,32,16,"FRESLEST AV BARS");
 }else{
  for(int y=16;y<224;y+=16)rect(c,16,y,288,1,109);
  for(int x=16;x<304;x+=16)rect(c,x,16,1,208,109);
  rect(c,24,28,272,36,0);label(c,32,38,pattern==2?"FRESLEST AV TEXT":"FRESLEST AV MONO");
  if(pattern==2){
   rect(c,24,80,272,76,0);label(c,32,88,"APPS SETTINGS");label(c,32,116,"SYSTEM INFO TEST");
   for(int i=0;i<48;i++)rect(c,32+i*5,168,2,20,255);
  }
 }
 for(unsigned i=0;i<8;i++){unsigned grey=(i<<5)|(i<<2)|(i>>1);rect(c,16+i*36,200,36,20,grey);}
 rect(c,8,8,304,2,255);rect(c,8,230,304,2,255);rect(c,8,8,2,224,255);rect(c,310,8,2,224,255);
}
