#pragma once
#include <LovyanGFX.hpp>

extern LGFX lcd;

inline void menuFadeIn() {
  for (int i = 0; i <= 255; i += 15) {
    lcd.setBrightness(i);
    delay(6);
  }
}

inline void menuFadeOut() {
  for (int i = 255; i >= 0; i -= 15) {
    lcd.setBrightness(i);
    delay(6);
  }
}
