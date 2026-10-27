#include "apps.h"
#include "ui_layout.h"
#include "../../gfx.h"
#include "../../input.h"
#include "settings.h"
#include "../../usb_input.h"
#include "audio_manager.h"
#include <freslest_video.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>

#include "ble_advertising.h"

// Compact settings adapter: shared with Wi-Fi pages.
#include "settings_canvas.h"

// ===== Settings menu =====
static const char* SET_ITEMS[] = { "Brightness", "Theme", "WiFi", "Bluetooth", "Sound", "Flash Reset", "Display" };
static constexpr int SET_COUNT = sizeof(SET_ITEMS)/sizeof(SET_ITEMS[0]);
static int setIndex = 0;

static void drawSettingsRow(int i,bool selected){
 int w=tft.width(),row=uiMenuRowHeight(w,34),x=uiMenuLeft(w),y=uiMenuTop(w)+i*row;
 tft.fillRect(x-2,y-2,w-2*x+4,row,TH().bg);
 if(selected){tft.fillRoundRect(x,y,w-2*x,uiCompact(w)?row-2:30,6,TH().selbg);tft.drawRoundRect(x,y,w-2*x,uiCompact(w)?row-2:30,6,TH().accent);}
 tft.setTextSize(2);tft.setTextColor(selected?TH().text:TH().dim,selected?TH().selbg:TH().bg);
 tft.setCursor(uiMenuTextX(w),y+uiMenuTextY(w,8));tft.print(selected?"> ":"  ");tft.print(SET_ITEMS[i]);
}

void drawSettingsMenuFull() {
  tft.fillScreen(TH().bg);
  drawHeader("Settings");
  drawFooter("UP/DOWN Move  OK Enter", "BACK Menu");
  clearBody();
  for (int i=0;i<SET_COUNT;i++) drawSettingsRow(i, i==setIndex);
}

void drawSettingsMenuSelectionOnly(int oldIdx, int newIdx) {
  drawSettingsRow(oldIdx, false);
  drawSettingsRow(newIdx, true);
  drawFooter("UP/DOWN Move  OK Enter", "BACK Menu");
}

// ===== Brightness =====
static void drawBrightnessBar();
void setBrightnessEnter() {
  tft.fillScreen(TH().bg);
  drawHeader("Settings: Brightness");
  drawFooter("L/R Adjust", "BACK Save+Menu");
  clearBody();

  settingTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  settingCursor(20, 60);
  tft.print(freslest_video_output()==FRESLEST_DISPLAY_VGA?"LCD backlight only. VGA uses the same LCD frame.":"Real backlight PWM (BL via MOSFET).");
  drawBrightnessBar();
}

static void drawBrightnessBar() {
  int x=40,y=120,w=400,h=30;
  settingFillRoundRect(x, y, w, h, 10, TH().panel);
  settingDrawRoundRect(x, y, w, h, 10, TH().dim);
  int fillW = (brightnessUI * (w-8)) / 255;
  settingFillRoundRect(x+4, y+4, fillW, h-8, 8, TH().accent);
  tft.setTextColor(TH().text, TH().panel);
  settingTextSize(2);
  settingCursor(40, 170);
  tft.print("Value: ");
  tft.print(brightnessUI);
}

void setBrightnessLoop(uint32_t now) {
  (void)now;
  bool redraw=false;
  if(settingHit(40,120,400,30)){brightnessUI=constrain((settingPointerX()-40)*255/399,0,255);redraw=true;}
  if (bLeft.pressed())  { brightnessUI = max(0, brightnessUI - 8); redraw=true; }
  if (bRight.pressed()) { brightnessUI = min(255, brightnessUI + 8); redraw=true; }
  if (redraw) {
    settingsSave();
    backlightSet((uint8_t)brightnessUI);
    setBrightnessEnter();
    drawBrightnessBar();
    if (soundEnabled) audioClick();
  }
}

// ===== Theme =====
void setThemeEnter() {
  tft.fillScreen(TH().bg);
  drawHeader("Settings: Theme");
  drawFooter("OK Toggle", "BACK Save+Menu");
  clearBody();

  settingTextSize(2);
  tft.setTextColor(TH().text, TH().bg);
  settingCursor(40, 110);
  tft.print("Theme: ");
  tft.setTextColor(TH().accent, TH().bg);
  tft.print(darkMode ? "Dark" : "Light");

  settingTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  settingCursor(40, 150);
  tft.print("OK changes theme instantly.");
}

void setThemeLoop(uint32_t now) {
  (void)now;
  if(settingHit(40,100,360,70))bOk.pressedEdge=true;
  if (bOk.pressed()) {
    darkMode = !darkMode;
    setThemeEnter();
  }
}

// Wi-Fi setup and its persistent station service are in wifi_manager.cpp.

// ===== Bluetooth =====



static void bleStart(){freslestBleStart();}
static void bleStop(){freslestBleStop();}
static int btRow=0;
static uint32_t btRevision=~0u;
static bool btForgetConfirm=false;
static void drawBtScreen(){
  auto s=freslestBleInfo();btRevision=s.revision;
  if(btRow>s.count)btRow=s.count;
  tft.fillScreen(TH().bg);drawHeader("Settings: Bluetooth");clearBody();
  drawFooter("UP/DOWN Select  OK Pair", "BACK Save+Menu");
  settingTextSize(1);tft.setTextColor(TH().text,TH().bg);settingCursor(20,52);
  tft.printf("BLE: %s  Connected: %u  L/R: Off/On",btEnabled?"ON":"OFF",s.connected);
  settingCursor(20,70);tft.print("Saved devices reconnect. FIRE: scan.");
  if(s.pairMode){
    settingCursor(40,112);tft.print(s.pairMode==2?"Confirm the same code on both devices:":"Type this code on the Bluetooth keyboard:");
    settingTextSize(3);settingCursor(tft.width()==320?186:150,150);tft.printf("%06lu",(unsigned long)s.passkey);
    settingTextSize(1);settingCursor(40,204);tft.print(s.pairMode==2?"OK Confirm    BACK Cancel":"Then press ENTER on that keyboard.");return;
  }
  if(btForgetConfirm){
    settingCursor(30,130);tft.print("Disconnect BLE devices and forget all bonds?");
    settingCursor(30,164);tft.print("OK Forget    LEFT Cancel");return;
  }
  for(unsigned i=0;i<=s.count;i++){
    int y=91+i*23;bool chosen=(int)i==btRow;
    settingFillRoundRect(18,y,444,21,4,chosen?TH().selbg:TH().bg);
    tft.setTextColor(TH().text,chosen?TH().selbg:TH().bg);settingCursor(26,y+6);
    if(i==s.count)tft.print("Forget all paired devices...");
    else tft.printf("%c %s%s",chosen?'>':' ',s.devices[i].name,s.devices[i].connected?" [Connected]":s.devices[i].remembered?" [Saved]":"");
  }
  tft.setTextColor(TH().dim,TH().bg);settingCursor(20,260);
  tft.print(s.status[0]?s.status:"Turn Bluetooth ON to find a keyboard or mouse.");
  settingCursor(20,279);tft.print("BLE: OS/NES. Doom: USB/buttons.");
}
void setBtEnter(){
  btRow=0;btForgetConfirm=false;
  if(btEnabled){bleStart();freslestBleScan();}else bleStop();drawBtScreen();
}
void setBtLoop(uint32_t now){
  (void)now;auto s=freslestBleInfo();bool redraw=false;
  if(s.pairMode){if(bOk.pressed()&&s.pairMode==2)freslestBlePairReply(true);}
  else if(btForgetConfirm){
    if(bOk.pressed()){freslestBleForget();btForgetConfirm=false;redraw=true;}
    if(bLeft.pressed()){btForgetConfirm=false;redraw=true;}
  }else{
    if(bLeft.pressed()||bRight.pressed()){
      btEnabled=bRight.pressed();settingsSave();if(btEnabled)bleStart();else bleStop();redraw=true;
    }
    if(bFire.pressed())freslestBleScan();
    if(bUp.pressed()){btRow=(btRow+s.count)%(s.count+1);redraw=true;}
    if(bDown.pressed()){btRow=(btRow+1)%(s.count+1);redraw=true;}
    bool open=bOk.pressed();
    if(settingHit(18,91,444,(s.count+1)*23)){btRow=(settingPointerY()-91)/23;open=true;redraw=true;}
    if(open){if(btRow==(int)s.count)btForgetConfirm=true;else freslestBleConnect(btRow);redraw=true;}
  }
  if(redraw||s.revision!=btRevision)drawBtScreen();
}

// ===== Flash Reset =====
static int resetSel = 0; // 0 No, 1 Yes
static void drawResetScreen() {
  tft.fillScreen(TH().bg);
  drawHeader("Settings: Flash Reset");
  drawFooter("L/R Select", "OK Confirm  BACK Cancel");
  clearBody();

  settingTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  settingCursor(20, 70);
  tft.print("This will reset: theme, brightness, WiFi, Bluetooth, and Note file.");

  int y=120;
  auto drawBtn=[&](int x,const char* txt,bool sel){
    uint16_t fill = sel ? TH().selbg : TH().panel;
    settingFillRoundRect(x, y, 180, 50, 12, fill);
    settingDrawRoundRect(x, y, 180, 50, 12, sel ? TH().accent : TH().dim);
    settingTextSize(2);
    tft.setTextColor(TH().text, fill);
    settingCursor(x+55, y+16);
    tft.print(txt);
  };
  drawBtn(60, "NO", resetSel==0);
  drawBtn(240, "YES", resetSel==1);
}

void setFlashResetEnter() {
  resetSel = 0;
  drawResetScreen();
}

void setFlashResetLoop(uint32_t now) {
  (void)now;
  if(settingHit(60,120,180,50)){resetSel=0;bOk.pressedEdge=true;}
  if(settingHit(240,120,180,50)){resetSel=1;bOk.pressedEdge=true;}
  if (bLeft.pressed() || bRight.pressed()) {
    resetSel ^= 1;
    drawResetScreen();
  }
  if (bOk.pressed() && resetSel==1) {
    settingsFactoryReset();
    settingsSave();
    drawResetScreen();
    settingTextSize(2);
    tft.setTextColor(TH().accent, TH().bg);
    settingCursor(20, 200);
    tft.print("RESET DONE!");
  }
}

// ===== Entry points for main settings menu =====
AppId settingsMenuToApp(int idx) {
  switch(idx){
    case 0: return APP_SET_BRIGHTNESS;
    case 1: return APP_SET_THEME;
    case 2: return APP_SET_WIFI;
    case 3: return APP_SET_BT;
    case 4: return APP_SET_SOUND;
    case 5: return APP_SET_FLASHRESET;
    case 6: return APP_SET_DISPLAY;
  }
  return APP_SETTINGS;
}

// allow apps.cpp to use setIndex
int& __settingsIndex() { return setIndex; }
int  __settingsCount() { return SET_COUNT; }


// ===== Sound =====
static void drawSoundDiagnostics() {
  settingFillRect(36,222,440,62,TH().bg);
  settingTextSize(1);tft.setTextColor(TH().dim,TH().bg);
  AudioDiagnostics d=audioGetDiagnostics();
  settingCursor(40,226);tft.printf("UART1 TX18 RX17 9600 | TX:%lu RX:%lu",(unsigned long)d.txPackets,(unsigned long)d.rxPackets);
  settingCursor(40,244);
  if(!d.ready)tft.print("Initializing DFPlayer / SD card...");
  else if(d.replySeen)tft.printf("Module replied | SD:%s | files:%u",d.cardKnown?(d.cardPresent?"OK":"absent"):"unknown",d.fileCount);
  else tft.print("No RX reply yet; test playback and check TX/GND.");
  settingCursor(40,262);tft.printf("Last MP3:%04u | error:%u | TX failures:%lu",d.lastTrack,d.lastError,(unsigned long)d.txFailures);
}

void setSoundEnter() {
  tft.fillScreen(TH().bg);
  drawHeader("Settings: Sound");
  drawFooter("L/R Volume  OK Mute", "BACK Save+Menu");
  clearBody();

  settingTextSize(2);
  tft.setTextColor(TH().text, TH().bg);
  settingCursor(30, 80);
  tft.print("Sound: ");
  tft.setTextColor(TH().accent, TH().bg);
  tft.print(soundEnabled ? "ON" : "OFF");

  // volume bar
  int x=40,y=140,w=400,h=26;
  settingFillRoundRect(x, y, w, h, 10, TH().panel);
  settingDrawRoundRect(x, y, w, h, 10, TH().dim);
  int fillW = (soundVolume * (w-8)) / 255;
  settingFillRoundRect(x+4, y+4, fillW, h-8, 8, TH().accent);

  settingTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  settingCursor(40, 180);
  tft.printf("Volume: %d", soundVolume);
  settingCursor(40, 202);tft.print("UP: Test MP3 | DOWN: UART diagnostics");
  drawSoundDiagnostics();
}

void setSoundLoop(uint32_t now) {
  static uint32_t lastStatus=0;
  if(bUp.pressed())audioTest();
  if(bDown.pressed())audioPrintDiagnostics();
  bool redraw = false;
  if(settingHit(30,72,420,32))bOk.pressedEdge=true;
  if(settingHit(40,140,400,26)){soundVolume=constrain((settingPointerX()-40)*255/399,0,255);redraw=true;}
  if(settingHit(40,194,400,22))bUp.pressedEdge=true;
  if (bLeft.pressed())  { soundVolume = max(0, soundVolume - 10); redraw = true; }
  if (bRight.pressed()) { soundVolume = min(255, soundVolume + 10); redraw = true; }
  if (bOk.pressed())    { soundEnabled = !soundEnabled; redraw = true; }

  if (redraw) {
    settingsSave();
    // apply immediately
    soundApply();
    setSoundEnter();
    // small feedback only if turning ON or adjusting volume (avoid sound when muted)
    if (soundEnabled) audioClick();
  }
  if(now-lastStatus>=500) {lastStatus=now;drawSoundDiagnostics();}
}




void setBtExit() {
  // keep BLE state according to btEnabled; stopping saves power when leaving page.
  freslestBlePairReply(false);
  if (!btEnabled) bleStop();
}

// ===== Display output =====
static int displayChoice=0;
static bool displayConfirm=false,displaySaveError=false;
static void drawDisplayPage(){
 tft.fillScreen(TH().bg);drawHeader("Settings: Display");clearBody();
 drawFooter(displayConfirm?"OK Restart + apply":"UP/DOWN Select  OK Apply","BACK Cancel");
 const char* names[]={
#if FRESLEST_LCD_MODEL == 9341
 "ILI9341 LCD (320x240)",
#else
 "ILI9488 LCD (480x320)",
#endif
 "VGA RGB332 (640x480)",usbInputViewerMode()?"USB PC Viewer: ON":"USB PC Viewer: OFF"};
 settingTextSize(2);
 for(int i=0;i<3;i++){
  int y=58+i*31;settingFillRoundRect(20,y,440,28,8,i==displayChoice?TH().selbg:TH().panel);
  tft.setTextColor(TH().text,i==displayChoice?TH().selbg:TH().panel);settingCursor(34,y+9);
  tft.print(i==displayChoice?"> ":"  ");tft.print(names[i]);
 }
 settingTextSize(1);tft.setTextColor(TH().dim,TH().bg);
 settingCursor(24,224);tft.print(tft.width()==320?"BACK+RESET: LCD | OK: apply":"BACK+RESET: LCD. VGA: same LCD image, RGB332.");
 settingFillRoundRect(20,236,440,34,6,TH().panel);settingCursor(32,248);
 tft.print(displayConfirm?"Restart and apply? OK confirms.":"OK Apply / Open selected display settings");
 if(displaySaveError){settingCursor(24,279);tft.print("Could not save. Try again; output has not changed.");}
}
void setDisplayEnter(){displayChoice=freslest_video_output()==FRESLEST_DISPLAY_VGA?1:0;displayConfirm=false;displaySaveError=false;drawDisplayPage();}
void setDisplayLoop(uint32_t now){
 (void)now;
 if(settingHit(20,58,440,93)){
  displayChoice=(settingPointerY()-58)/31;if(displayChoice>2)displayChoice=2;
  displayConfirm=false;displaySaveError=false;
  drawDisplayPage();return;
 }
 if(bUp.pressed()||bDown.pressed()||bLeft.pressed()||bRight.pressed()){
  int step=(bUp.pressed()||bLeft.pressed())?-1:1;
  displayChoice=(displayChoice+step+3)%3;displayConfirm=false;displaySaveError=false;drawDisplayPage();return;
 }
 if(settingHit(20,236,440,34))bOk.pressedEdge=true;
 if(!bOk.pressed())return;
 if(displayChoice<2&&(displayChoice==0?FRESLEST_DISPLAY_LCD:FRESLEST_DISPLAY_VGA)==freslest_video_output()){enterApp(APP_SETTINGS);return;}
 if(!displayConfirm){displayConfirm=true;drawDisplayPage();return;}
 if(displayChoice==2){if(!usbInputSwitchViewer(!usbInputViewerMode())){displaySaveError=true;drawDisplayPage();}return;}
 if(!freslest_video_save_output(displayChoice==0?FRESLEST_DISPLAY_LCD:FRESLEST_DISPLAY_VGA)){displaySaveError=true;drawDisplayPage();return;}
 audioStop();tft.fillScreen(0);drawHeader("Restarting display...");gfxPresent();delay(300);ESP.restart();
}

