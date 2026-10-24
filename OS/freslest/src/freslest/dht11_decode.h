#pragma once
#include <stdint.h>
inline bool dht11Decode(const uint8_t data[5],float& temperature,float& humidity) {
  if((uint8_t)(data[0]+data[1]+data[2]+data[3])!=data[4])return false;
  float h=data[0]+data[1]*0.1f;
  float t=(data[2]&0x7f)+(data[3]&0x7f)*0.1f;
  if(data[2]&0x80)t=-t;
  if(h<0||h>100||t<-20||t>80)return false;
  temperature=t;humidity=h;return true;
}
