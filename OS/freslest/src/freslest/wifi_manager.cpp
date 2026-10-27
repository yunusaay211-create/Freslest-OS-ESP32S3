#include "wifi_manager.h"
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "settings_canvas.h"
#include "apps.h"
#include "settings.h"
#include "text_entry.h"
#include "power_recovery.h"
#include <WebServer.h>
#include <WiFi.h>
#include <cstring>
#include <esp_wifi.h>
#include <esp_heap_caps.h>
namespace {
WebServer portal(80);
bool portalOn = false, begun = false, requested = true, lastEnabled = false,
     attempting = false, wasConnected = false;
bool startupAllowed=false;
uint32_t connectAt = 0, lastDraw = 0;
enum View { Main, Networks };
View view = Main;
int row = 0, networkRow = 0, networkTop = 0, networkCount = 0;
bool scanning = false;
enum Edit { NoEdit, ManualSsid, Password };
Edit editing = NoEdit;
char selectedSsid[33]{};
char wifiMessage[80] = "";
String escape(const String &s) {
  String out;
  out.reserve(s.length());
  for (char c : s) {
    if (c == '&')
      out += "&amp;";
    else if (c == '<')
      out += "&lt;";
    else if (c == '>')
      out += "&gt;";
    else if (c == '\"')
      out += "&quot;";
    else if (c == '\'')
      out += "&#39;";
    else
      out += c;
  }
  return out;
}
void message(const char *s) { strlcpy(wifiMessage, s, sizeof(wifiMessage)); }
void startPortal() {
  if (portalOn)
    return;
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP("Freslest-Setup")) {
    message("Setup AP could not start.");
    return;
  }
  portal.on("/", HTTP_GET, []() {
    String page =
        "<!doctype html><html><head><meta charset=utf-8><meta name=viewport "
        "content='width=device-width,initial-scale=1'><title>Freslest "
        "Setup</title></head><body "
        "style='font-family:sans-serif;max-width:600px;margin:24px "
        "auto;padding:12px'><h1>Freslest Setup</h1>";
    page += "<h2>Wi-Fi</h2><form method=post action=/wifi><label>Network name "
            "(SSID)<br><input name=ssid maxlength=32 required value=\"" +
            escape(wifiSSID) +
            "\"></label><p><label>Password<br><input name=pass type=password "
            "maxlength=64 autocomplete=new-password></label></p><p>Enter the "
            "Wi-Fi password again when saving. Leave blank for an open "
            "network.</p><button>Save Wi-Fi and connect</button></form>";
    page += "<p>Apps &gt; Browser connects directly from ESP32. No PC or "
            "server setup is needed.</p></body></html>";
    portal.send(200, "text/html", page);
  });
  portal.on("/wifi", HTTP_POST, []() {
    String ssid = portal.arg("ssid"), pass = portal.arg("pass");
    if (!ssid.length() || ssid.length() > 32 || pass.length() > 64) {
      portal.send(
          400, "text/plain",
          "SSID must contain 1..32 bytes; password maximum is 64 bytes.");
      return;
    }
    settingsSetWifiCreds(ssid, pass);
    wifiManagerRequest();
    portal.send(200, "text/html",
                "<h2>Wi-Fi saved</h2><p>Freslest is connecting. The connection "
                "stays on outside Settings.</p><a href='/'>Back</a>");
  });
  portal.onNotFound([]() {
    portal.send(404, "text/plain",
                "Open http://192.168.4.1/ for Freslest setup.");
  });
  portal.begin();
  portalOn = true;
  message("Phone setup is running at 192.168.4.1.");
}
void stopPortal() {
  if (!portalOn)
    return;
  portal.stop();
  WiFi.enableAP(false);
  portalOn = false;
  message("Phone setup stopped. Station Wi-Fi remains enabled.");
}
void beginConnect(uint32_t now) {
  if (!wifiEnabled || !wifiSSID.length())
    return;
  bool retry=connectAt!=0;
  connectAt = now;
  attempting = true;
  requested = false;
  if(wifiSSID.length()>32 || wifiPASS.length()>64){
    message("Saved Wi-Fi settings invalid. Enter the network again.");
    freslestSafetyError("Saved Wi-Fi settings exceed SSID/password limits.");return;
  }
  freslestSafetyStage(FreslestBootStage::Wifi);
  // One retry owner. Do not race Arduino auto-reconnect with our timer or
  // disconnect a newly starting STA immediately after enabling it at boot.
  WiFi.setAutoReconnect(false);
  if(!WiFi.enableSTA(true)){
    message("Wi-Fi could not start. Menu remains available.");
    freslestSafetyError("Wi-Fi STA startup failed; check See Log / UART.");return;
  }
  if(retry || WiFi.status()==WL_CONNECTED)WiFi.disconnect(false,false);
  if(WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str())==WL_CONNECT_FAILED){
    message("Wi-Fi connection failed. Retrying in 20 seconds.");
    freslestSafetyError("Wi-Fi.begin failed; retry scheduled in 20 seconds.");return;
  }
  message("Connecting to saved network...");
}
void drawMain() {
  gfxSetCompactUi(true);
  tft.fillScreen(TH().bg);
  drawHeader("Settings: WiFi");
  clearBody();
  drawFooter("UP/DOWN Select OK Open", "BACK Settings");
  settingTextSize(1);
  tft.setTextColor(TH().text, TH().bg);
  settingCursor(20, 52);
  if (WiFi.status() == WL_CONNECTED)
    tft.printf(settingCompact()?"Connected: %.16s  %s":"Connected: %.32s  IP: %s", wifiSSID.c_str(),
               WiFi.localIP().toString().c_str());
  else
    tft.print(wifiEnabled ? "Not connected" : "Station Wi-Fi is OFF");
  const char *names[] = {"WiFi:", "Scan and choose a network",
                         "Connect saved network", "Add network manually",
                         "Phone setup portal:"};
  for (int i = 0; i < 5; i++) {
    int y = 78 + i * 34;
    bool sel = i == row;
    settingFillRoundRect(18, y, 444, 30, 5, sel ? TH().selbg : TH().panel);
    tft.setTextColor(TH().text, sel ? TH().selbg : TH().panel);
    settingCursor(28, y + 10);
    tft.print(names[i]);
    if (i == 0)
      tft.print(wifiEnabled ? " ON" : " OFF");
    if (i == 4)
      tft.print(portalOn ? " ON (192.168.4.1)" : " OFF");
  }
  tft.setTextColor(TH().dim, TH().bg);
  settingCursor(20, 258);
  tft.printf("%.48s",wifiMessage);
  settingCursor(20, 279);
  tft.print("Wi-Fi stays connected when you leave this page.");
}
void drawNetworks() {
  gfxSetCompactUi(true);
  tft.fillScreen(TH().bg);
  drawHeader("WiFi: Networks");
  clearBody();
  drawFooter("UP/DOWN Select OK Join", "FIRE Scan BACK Exit");
  settingTextSize(1);
  tft.setTextColor(TH().text, TH().bg);
  settingCursor(20, 54);
  if (scanning) {
    tft.print("Scanning... existing Wi-Fi stays on.");
    return;
  }
  if (networkCount <= 0) {
    tft.print("No networks found. FIRE retries; BACK returns.");
    return;
  }
  if (networkRow < networkTop)
    networkTop = networkRow;
  if (networkRow >= networkTop + 6)
    networkTop = networkRow - 5;
  for (int n = networkTop; n < networkCount && n < networkTop + 6; n++) {
    int y = 78 + (n - networkTop) * 32;
    bool sel = n == networkRow;
    settingFillRoundRect(18, y, 444, 29, 5, sel ? TH().selbg : TH().panel);
    tft.setTextColor(TH().text, sel ? TH().selbg : TH().panel);
    settingCursor(28, y + 10);
    tft.printf(settingCompact()?"%s %.24s %ld":"%s %.32s  %ld dBm",
               WiFi.encryptionType(n) == WIFI_AUTH_OPEN ? "[Open]" : "[Lock]",
               WiFi.SSID(n).c_str(), (long)WiFi.RSSI(n));
  }
  tft.setTextColor(TH().dim, TH().bg);
  settingCursor(20, 279);
  tft.printf(settingCompact()?"%d networks. OK Join / BACK Return":"%d networks. Password entry supports buttons and keyboards.",
             networkCount);
}
void scanNetworks() {
  if (scanning && WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    view = Networks;
    drawNetworks();
    return;
  }
  if (!wifiEnabled)
    wifiManagerSetEnabled(true);
  WiFi.enableSTA(true);
  WiFi.scanDelete();
  int n = WiFi.scanNetworks(true, true);
  scanning = n == WIFI_SCAN_RUNNING;
  networkCount = n >= 0 ? n : 0;
  networkRow = networkTop = 0;
  view = Networks;
  drawNetworks();
}
void join(const char *password) {
  settingsSetWifiCreds(String(selectedSsid), String(password));
  wifiManagerRequest();
  view = Main;
  editing = NoEdit;
  drawMain();
}
} // namespace
void wifiManagerBegin() {
  if (begun)
    return;
  begun = true;
  lastEnabled = wifiEnabled;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  // Hardware startup is released after the first visible, responsive menu.
}
void wifiManagerAllowStartup(){startupAllowed=true;}
void wifiManagerRequest() {
  requested = true;
  attempting = false;
}
void wifiManagerSetEnabled(bool on) {
  wifiEnabled = on;
  settingsSave();
  lastEnabled = on;
  requested = on;
  attempting = false;
  if (!on) {
    WiFi.disconnect(false, false);
    WiFi.enableSTA(false);
    message("Station Wi-Fi is off. Saved credentials are retained.");
  }
}
bool wifiPortalActive() { return portalOn; }
void wifiManagerUpdate(uint32_t now) {
  if (!begun)
    wifiManagerBegin();
  if(!startupAllowed)return;
  if (portalOn)
    portal.handleClient();
  if (app == APP_CAMERA) {
    if (portalOn)
      stopPortal();
    wasConnected = false;
    attempting = false;
    requested = wifiEnabled;
    return; // Camera intentionally uses its own network.
  }
  if (lastEnabled != wifiEnabled) {
    lastEnabled = wifiEnabled;
    requested = wifiEnabled;
    attempting = false;
    if (!wifiEnabled) {
      WiFi.disconnect(false, false);
      WiFi.enableSTA(false);
    }
  }
  if (!wifiEnabled || !wifiSSID.length())
    return;
  if (requested && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    beginConnect(now);
    return;
  }
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    if (!wasConnected)
      message("Connected. Wi-Fi stays on outside Settings.");
    wasConnected = true;
    attempting = false;
    requested = false;
    return;
  }
  if (wasConnected) {
    wasConnected = false;
    attempting = true;
    connectAt = now - 14000u;
  }
  if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    return;
  if (requested || !attempting || (uint32_t)(now - connectAt) >= 20000u)
    beginConnect(now);
}
void setWifiEnter() {
  view = Main;
  row = 0;
  editing = NoEdit;
  lastDraw = 0;
  if (!wifiSSID.length())
    startPortal();
  drawMain();
}
void setWifiLoop(uint32_t now) {
  char value[384];
  auto edited = textEntryTakeResult(value, sizeof(value));
  if (editing != NoEdit && edited != TextEntryResult::None) {
    Edit field = editing;
    editing = NoEdit;
    if (edited == TextEntryResult::Accepted) {
      if (field == ManualSsid && value[0]) {
        strlcpy(selectedSsid, value, sizeof(selectedSsid));
        editing = Password;
        textEntryBegin("WiFi: Password", "", 64, true);
        return;
      }
      if (field == Password) {
        join(value);
        memset(value, 0, sizeof(value));
        return;
      }
    }
    if (view == Networks)
      drawNetworks();
    else
      drawMain();
  }
  if (view == Networks) {
    if (scanning) {
      int n = WiFi.scanComplete();
      if (n != WIFI_SCAN_RUNNING) {
        scanning = false;
        networkCount = n >= 0 ? n : 0;
        drawNetworks();
      }
    }
    if (bBack.pressed()) {
      view = Main;
      drawMain();
      return;
    }
    if (bFire.pressed()) {
      scanNetworks();
      return;
    }
    bool redraw = false;
    if (networkCount > 0) {
      if (bUp.pressed()) {
        networkRow = (networkRow + networkCount - 1) % networkCount;
        redraw = true;
      }
      if (bDown.pressed()) {
        networkRow = (networkRow + 1) % networkCount;
        redraw = true;
      }
      bool open = bOk.pressed();
      if (settingHit(18, 78, 444, 192)) {
        int n = networkTop + (settingPointerY() - 78) / 32;
        if (n < networkCount) {
          networkRow = n;
          open = true;
        }
      }
      if (open && !scanning) {
        String ssid = WiFi.SSID(networkRow);
        if (!ssid.length() || ssid.length() > 32) {
          message("Invalid network name.");
          return;
        }
        strlcpy(selectedSsid, ssid.c_str(), sizeof(selectedSsid));
        if (WiFi.encryptionType(networkRow) == WIFI_AUTH_OPEN) {
          join("");
          return;
        }
        editing = Password;
        textEntryBegin("WiFi: Password", "", 64, true);
        return;
      }
    }
    if (redraw)
      drawNetworks();
    return;
  }
  if (bBack.pressed()) {
    setWifiExit();
    enterApp(APP_SETTINGS);
    return;
  }
  bool redraw = false;
  if (bUp.pressed()) {
    row = (row + 4) % 5;
    redraw = true;
  }
  if (bDown.pressed()) {
    row = (row + 1) % 5;
    redraw = true;
  }
  bool open = bOk.pressed();
  if (settingHit(18, 78, 444, 170)) {
    row = (settingPointerY() - 78) / 34;
    open = true;
  }
  if (bFire.pressed()) {
    scanNetworks();
    return;
  }
  if (open || (row == 0 && (bLeft.pressed() || bRight.pressed()))) {
    if (row == 0) {
      wifiManagerSetEnabled(bRight.pressed()  ? true
                            : bLeft.pressed() ? false
                                              : !wifiEnabled);
      redraw = true;
    } else if (row == 1) {
      scanNetworks();
      return;
    } else if (row == 2) {
      wifiManagerSetEnabled(true);
      wifiManagerRequest();
      message(wifiSSID.length() ? "Connecting to saved network..."
                                : "No saved network. Scan or add one.");
      redraw = true;
    } else if (row == 3) {
      editing = ManualSsid;
      textEntryBegin("WiFi: Network name", "", 32);
      return;
    } else {
      if (portalOn)
        stopPortal();
      else
        startPortal();
      redraw = true;
    }
  }
  if (redraw || (uint32_t)(now - lastDraw) >= 1000u) {
    lastDraw = now;
    drawMain();
  }
}
void setWifiExit() {
  textEntryCancel();
  editing = NoEdit;
  settingsSave();
}
void wifiManagerDiagnostics() {
  Serial.printf("WiFi memory: INTERNAL free=%u largest=%u; PSRAM free=%u largest=%u\n",
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),
    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  Serial.printf("WiFi enabled=%u status=%d portal=%u ip=%s\n", wifiEnabled,
                (int)WiFi.status(), portalOn,
                WiFi.localIP().toString().c_str());
}
