#include "settings.h"
#include <LittleFS.h>
#include <Preferences.h>

Theme darkT  { 0x0000, 0x10A2, 0xFFFF, 0xA514, 0x07FF, 0x2104 };
Theme lightT { 0xFFFF, 0xCE59, 0x0000, 0x7BEF, 0x03EF, 0xE71C };

bool darkMode = true;
int  brightnessUI = 200;

extern bool soundEnabled;
int soundVolume = 180;

bool wifiEnabled = false;
bool btEnabled   = false;
bool nesFullscreen=false;

String wifiSSID = "";
String wifiPASS = "";

static Preferences prefs;
static constexpr const char* PREF_NS = "freslest";

Theme& TH() { return darkMode ? darkT : lightT; }

void settingsLoad() {
  prefs.begin(PREF_NS, true);
  darkMode     = prefs.getBool("dark", darkMode);
  brightnessUI = prefs.getInt("blui", brightnessUI);
  wifiEnabled  = prefs.getBool("wifi", wifiEnabled);
  btEnabled    = prefs.getBool("bt", btEnabled);
  nesFullscreen=prefs.getBool("nes_full",false);
  soundEnabled = prefs.getBool("snd", soundEnabled);
  soundVolume  = prefs.getInt("vol", soundVolume);
  wifiSSID     = prefs.getString("ssid", wifiSSID);
  wifiPASS     = prefs.getString("pass", wifiPASS);
  prefs.end();
}

void settingsSave() {
  prefs.begin(PREF_NS, false);
  prefs.putBool("dark", darkMode);
  prefs.putInt("blui", brightnessUI);
  prefs.putBool("wifi", wifiEnabled);
  prefs.putBool("bt", btEnabled);
  prefs.putBool("nes_full",nesFullscreen);
  prefs.putBool("snd", soundEnabled);
  prefs.putInt("vol", soundVolume);
  prefs.putString("ssid", wifiSSID);
  prefs.putString("pass", wifiPASS);
  prefs.end();
}

void settingsSetWifiCreds(const String& ssid, const String& pass) {
  wifiSSID = ssid;
  wifiPASS = pass;
  wifiEnabled = (wifiSSID.length() > 0);
  settingsSave();
}

void settingsFactoryReset() {
  // Preferences
  prefs.begin(PREF_NS, false);
  prefs.clear();
  prefs.end();

  // Reset runtime defaults
  darkMode = true;
  brightnessUI = 200;
  wifiEnabled = false;
  btEnabled = false;
  nesFullscreen=false;
  soundEnabled = true;
  soundVolume = 180;
  wifiSSID = "";
  wifiPASS = "";

  // Remove note file if exists
  if (LittleFS.begin(false)) {
    if (LittleFS.exists("/note.txt")) LittleFS.remove("/note.txt");
  }
}
