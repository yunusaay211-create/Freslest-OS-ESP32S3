#pragma once
#include <stdint.h>

enum ButtonMask : uint8_t {
  KeyUp=1,KeyDown=2,KeyLeft=4,KeyRight=8,KeyOk=16,KeyBack=32,KeyFire=64
};

struct ButtonSampler {
  uint8_t held=0,previousRaw=0;
  uint32_t changedAt[7]={};
  void reset(uint8_t raw,uint32_t now) {
    held=previousRaw=raw;
    for(auto& time:changedAt)time=now;
  }
  uint8_t sample(uint8_t raw,uint32_t now) {
    uint8_t pressed=0;
    for(unsigned i=0;i<7;++i) {
      uint8_t bit=(uint8_t)(1u<<i);
      if((raw^previousRaw)&bit)changedAt[i]=now;
      if(((raw^held)&bit)&&now-changedAt[i]>=25) {
        held^=bit;if(held&bit)pressed|=bit;
      }
    }
    previousRaw=raw;return pressed;
  }
};

inline uint8_t nesPadForButtons(uint8_t held) {
  uint8_t pad=(held&15)<<4;
  if((held&(KeyOk|KeyUp))==(KeyOk|KeyUp))pad=(pad&~0x10)|0x08;
  else if((held&(KeyOk|KeyDown))==(KeyOk|KeyDown))pad=(pad&~0x20)|0x04;
  else if(held&KeyOk)pad|=0x01;
  if(held&KeyFire)pad|=0x02;
  return pad;
}
