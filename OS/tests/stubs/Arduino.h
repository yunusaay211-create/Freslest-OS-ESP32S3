#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
class String {};
struct MockDebugSerial {
  template<class... Args> int printf(const char*,Args...){return 0;}
  void println(const char*){}
};
inline MockDebugSerial Serial;
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
inline int testPins[64];
inline void pinMode(int,int){}
inline int digitalRead(int pin){return testPins[pin];}
extern uint32_t testMillis;
inline uint32_t millis(){return testMillis;}
template<class T> inline T constrain(T a,T lo,T hi){return a<lo?lo:a>hi?hi:a;}
