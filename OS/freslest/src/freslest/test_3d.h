#pragma once
#include <Arduino.h>

bool canRun3DTester();

// State-based 3D Tester (no while loops)
void test3dEnter();
void test3dLoop(uint32_t now);
void test3dExit();
bool test3dWantsExit();
