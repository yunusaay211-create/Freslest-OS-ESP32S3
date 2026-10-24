#pragma once
#include <Arduino.h>

void caineInit();
void caineUpdate(uint32_t now);
void caineOverlay();
void caineTouch(int x, int y);

// debug / control
void caineReset();
bool caineActive();
int caineGetStage();
bool caineNeedsFullScreen();
