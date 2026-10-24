#pragma once
#include <Arduino.h>

// 🌙 Timeout (idle süresi)
#define LOWPOWER_TIMEOUT_MS 30000   // 30 saniye

// 🌙 Global kontrol (oyun vs. kapatabilir)
extern bool allowSleep;

// 🔥 API
void LowPower_init();
void LowPower_update();
void LowPower_onUserInput();

bool LowPower_isActive();

// 🔌 Hooklar (UI / sistem sen dolduracaksın)
void LowPower_onEnter();  // sleep'e girince
void LowPower_onExit();   // sleep'ten çıkınca