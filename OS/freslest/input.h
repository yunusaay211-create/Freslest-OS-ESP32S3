#pragma once
#include <Arduino.h>

// Buttons: UP=4 DOWN=5 LEFT=6 RIGHT=7 OK=15 BACK=16
static constexpr int BTN_UP    = 4;
static constexpr int BTN_DOWN  = 5;
static constexpr int BTN_LEFT  = 6;
static constexpr int BTN_RIGHT = 7;
static constexpr int BTN_OK    = 15;
static constexpr int BTN_BACK  = 16;
static constexpr int BTN_FIRE  = 3;

struct Button {
  int pin = -1;
  bool stable = HIGH;
  bool lastRead = HIGH;
  uint32_t lastChange = 0;
  bool pressedEdge = false;

  void begin(int p);
  void update(uint32_t now, uint16_t db = 25);

  bool pressed() const { return pressedEdge; }
  bool down() const { return stable == LOW; }
};

extern Button bUp, bDown, bLeft, bRight, bOk, bBack, bFire;

void inputBegin();
void inputUpdate(uint32_t now);
void inputNesBegin();
uint8_t inputNesPad();

