// Freslest USB Screen protocol 1. RGB332 PackBits, no framebuffer ownership.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <freslest_cursor.h>
namespace freslest_screen {
constexpr size_t headerSize=24,maxPixels=480*360,maxPacket=headerSize+maxPixels+(maxPixels+127)/128;
inline uint8_t rgb332(uint16_t v){return ((v>>8)&0xe0)|((v>>6)&0x1c)|((v>>3)&3);}
inline uint32_t checksum(const uint8_t*p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
inline void put16(uint8_t*p,uint16_t v){p[0]=v;p[1]=v>>8;}
inline void put32(uint8_t*p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
inline size_t encode(uint8_t*out,size_t cap,const uint16_t*raw,unsigned w,unsigned h,unsigned div,bool swapped,bool cursor,int cx,int cy,uint32_t seq){
 if(!out||!raw||!w||!h||(div!=1&&div!=2)||w%div||h%div||w>480||h>360||cap<headerSize)return 0;
 unsigned ow=w/div,oh=h/div;size_t n=(size_t)ow*oh,pos=headerSize;
 auto sample=[&](size_t i){int x=(i%ow)*div,y=(i/ow)*div;uint16_t v=raw[y*w+x];if(swapped)v=(v>>8)|(v<<8);
  if(cursor){unsigned c=freslest_cursor_pixel(x-cx,y-cy);if(c)v=c==1?0:c==2?0xffff:0x057f;}return rgb332(v);};
 size_t i=0;
 while(i<n){uint8_t value=sample(i);size_t run=1;while(run<128&&i+run<n&&sample(i+run)==value)++run;
  if(run>=3){if(cap-pos<2)return 0;out[pos++]=0x80|(run-1);out[pos++]=value;i+=run;}
  else {size_t start=i;i+=run;while(i<n&&i-start<128){if(i+2<n&&sample(i)==sample(i+1)&&sample(i)==sample(i+2))break;++i;}
   size_t count=i-start;if(count>128){count=128;i=start+128;}if(cap-pos<count+1)return 0;
   out[pos++]=count-1;for(size_t k=0;k<count;++k)out[pos++]=sample(start+k);}
 }
 memcpy(out,"FSCRN1\r\n",8);put32(out+8,pos-headerSize);put32(out+12,seq);put16(out+16,ow);put16(out+18,oh);put32(out+20,checksum(out+headerSize,pos-headerSize));return pos;
}
}
