#pragma once
#include <stdint.h>
#include <stddef.h>
struct INesInfo {unsigned mapper,prgKB,chrKB;const char* error;};
inline bool parseINes(const uint8_t* h,size_t length,size_t fileSize,INesInfo& out) {
  out={0,0,0,"Invalid NES header"};
  if(length<16||h[0]!='N'||h[1]!='E'||h[2]!='S'||h[3]!=0x1a)return false;
  if((h[7]&0x0c)==0x08) {out.error="NES 2.0 is not supported";return false;}
  if(h[7]&3) {out.error="VS/PlayChoice is not supported";return false;}
  if(h[4]==0) {out.error="Empty PRG ROM";return false;}
  size_t required=16+(h[6]&4?512:0)+(size_t)h[4]*16384+(size_t)h[5]*8192;
  if(fileSize<required) {out.error="ROM is incomplete / truncated";return false;}
  out.mapper=(h[6]>>4)|(h[7]&0xf0);out.prgKB=h[4]*16;out.chrKB=h[5]*8;out.error=nullptr;return true;
}
