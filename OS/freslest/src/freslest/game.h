#pragma once
#include <Arduino.h>

struct DinoGame {
  bool running = true;
  int groundY = 250;
  int dinoY = 250;
  int v = 0;
  int obsX = 480;
  int score = 0;
  uint32_t lastTick = 0;

  int prevDinoY = 250;
  int prevObsX  = 480;
  int prevScore = -1;

  void reset();
  void jump();
  void drawStatic();
  void updatePhysics();
  void drawFramePartial();
};

extern DinoGame game;


