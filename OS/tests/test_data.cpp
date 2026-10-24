#include <cstdio>
#include <cassert>
#include <cmath>
#include <cstring>
#include "dht11_decode.h"
#include "ines_header.h"
int main() {
  float t=0,h=0;uint8_t d[]={55,0,24,3,82};assert(dht11Decode(d,t,h));assert(fabs(t-24.3f)<0.01&&h==55);d[4]++;assert(!dht11Decode(d,t,h));
  uint8_t header[16]={'N','E','S',0x1a,2,1,0,0};INesInfo info;
  assert(parseINes(header,16,40976,info)&&info.mapper==0&&info.prgKB==32&&info.chrKB==8);
  assert(!parseINes(header,16,40975,info));assert(!parseINes(header,8,40976,info));
  header[6]=4;assert(!parseINes(header,16,40976,info));assert(parseINes(header,16,41488,info));
  header[7]=8;assert(!parseINes(header,16,41488,info));header[7]=0;header[4]=0;assert(!parseINes(header,16,41488,info));
  puts("PASS: DHT11 checksum/decimal decode, iNES size/trainer/magic/format validation");
}
