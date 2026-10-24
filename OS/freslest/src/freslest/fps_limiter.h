#pragma once
#include <Arduino.h>

static unsigned long lastFrame = 0;
static const int TARGET_FPS = 60;

inline void limitFPS() {
  unsigned long now = millis();
  unsigned long frameTime = 1000 / TARGET_FPS;
  if (now - lastFrame < frameTime) {
    delay(frameTime - (now - lastFrame));
  }
  lastFrame = millis();
}
