#pragma once
#include <Arduino.h>

struct Theme {
  uint16_t bg, panel, text, dim, accent, selbg;
};

extern Theme darkT;
extern Theme lightT;

extern bool darkMode;
extern int  brightnessUI; // BL 3V3 olduğu için UI-only

Theme& TH();

void settingsLoad();
void settingsSave();

// ===== Sound =====
extern bool soundEnabled;
extern int  soundVolume; // 0..255

// ===== Connectivity / Reset =====
extern bool wifiEnabled;
extern bool btEnabled;
extern bool nesFullscreen;

extern String wifiSSID;
extern String wifiPASS;

void settingsSetWifiCreds(const String& ssid, const String& pass);
void settingsFactoryReset();   // theme/brightness/wifi/bt + note file
