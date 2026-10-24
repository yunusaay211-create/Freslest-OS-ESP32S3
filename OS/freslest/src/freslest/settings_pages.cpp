#include "apps.h"
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

// ===== Settings menu =====
static const char* SET_ITEMS[] = { "Brightness", "Theme", "WiFi", "Bluetooth", "Sound", "Flash Reset", "Display" };
static constexpr int SET_COUNT = sizeof(SET_ITEMS)/sizeof(SET_ITEMS[0]);
static int setIndex = 0;

static void drawSettingsRow(int i, bool selected) {
  int y = 54 + i * 34;
  tft.fillRect(18, y - 2, 444, 34, TH().bg);

  if (selected) {
    tft.fillRoundRect(20, y, 440, 30, 8, TH().selbg);
    tft.drawRoundRect(20, y, 440, 30, 8, TH().accent);
    tft.setTextColor(TH().text, TH().selbg);
    tft.setCursor(40, y + 8);
    tft.setTextSize(2);
    tft.print("> ");
    tft.print(SET_ITEMS[i]);
  } else {
    tft.setTextColor(TH().dim, TH().bg);
    tft.setCursor(40, y + 8);
    tft.setTextSize(2);
    tft.print("  ");
    tft.print(SET_ITEMS[i]);
  }
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

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(20, 60);
  tft.print(freslest_video_output()==FRESLEST_DISPLAY_COMPOSITE?"LCD backlight only. Adjust TV brightness on the TV.":"Real backlight PWM (BL via MOSFET).");
  drawBrightnessBar();
}

static void drawBrightnessBar() {
  int x=40,y=120,w=400,h=30;
  tft.fillRoundRect(x, y, w, h, 10, TH().panel);
  tft.drawRoundRect(x, y, w, h, 10, TH().dim);
  int fillW = (brightnessUI * (w-8)) / 255;
  tft.fillRoundRect(x+4, y+4, fillW, h-8, 8, TH().accent);
  tft.setTextColor(TH().text, TH().panel);
  tft.setTextSize(2);
  tft.setCursor(40, 170);
  tft.print("Value: ");
  tft.print(brightnessUI);
}

void setBrightnessLoop(uint32_t now) {
  (void)now;
  bool redraw=false;
  if(usbInputHit(40,120,400,30)){brightnessUI=constrain((usbPointer.x-40)*255/399,0,255);redraw=true;}
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

  tft.setTextSize(2);
  tft.setTextColor(TH().text, TH().bg);
  tft.setCursor(40, 110);
  tft.print("Theme: ");
  tft.setTextColor(TH().accent, TH().bg);
  tft.print(darkMode ? "Dark" : "Light");

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(40, 150);
  tft.print("OK changes theme instantly.");
}

void setThemeLoop(uint32_t now) {
  (void)now;
  if(usbInputHit(40,100,360,70))bOk.pressedEdge=true;
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
  tft.setTextSize(1);tft.setTextColor(TH().text,TH().bg);tft.setCursor(20,52);
  tft.printf("BLE: %s  Connected: %u   LEFT Off / RIGHT On",btEnabled?"ON":"OFF",s.connected);
  tft.setCursor(20,70);tft.print("Saved devices reconnect on wake. FIRE scans for new devices.");
  if(s.pairMode){
    tft.setCursor(40,112);tft.print(s.pairMode==2?"Confirm the same code on both devices:":"Type this code on the Bluetooth keyboard:");
    tft.setTextSize(3);tft.setCursor(150,150);tft.printf("%06lu",(unsigned long)s.passkey);
    tft.setTextSize(1);tft.setCursor(40,204);tft.print(s.pairMode==2?"OK Confirm    BACK Cancel":"Then press ENTER on that keyboard.");return;
  }
  if(btForgetConfirm){
    tft.setCursor(30,130);tft.print("Disconnect BLE devices and forget all bonds?");
    tft.setCursor(30,164);tft.print("OK Forget    LEFT Cancel");return;
  }
  for(unsigned i=0;i<=s.count;i++){
    int y=91+i*23;bool chosen=(int)i==btRow;
    tft.fillRoundRect(18,y,444,21,4,chosen?TH().selbg:TH().bg);
    tft.setTextColor(TH().text,chosen?TH().selbg:TH().bg);tft.setCursor(26,y+6);
    if(i==s.count)tft.print("Forget all paired devices...");
    else tft.printf("%c %s%s",chosen?'>':' ',s.devices[i].name,s.devices[i].connected?" [Connected]":s.devices[i].remembered?" [Saved]":"");
  }
  tft.setTextColor(TH().dim,TH().bg);tft.setCursor(20,260);
  tft.print(s.status[0]?s.status:"Turn Bluetooth ON to find a keyboard or mouse.");
  tft.setCursor(20,279);tft.print("BLE HID: OS / NES. Doom: USB / physical buttons.");
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
    if(usbInputHit(18,91,444,(s.count+1)*23)){btRow=(usbPointer.y-91)/23;open=true;redraw=true;}
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

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(20, 70);
  tft.print("This will reset: theme, brightness, WiFi, Bluetooth, and Note file.");

  int y=120;
  auto drawBtn=[&](int x,const char* txt,bool sel){
    uint16_t fill = sel ? TH().selbg : TH().panel;
    tft.fillRoundRect(x, y, 180, 50, 12, fill);
    tft.drawRoundRect(x, y, 180, 50, 12, sel ? TH().accent : TH().dim);
    tft.setTextSize(2);
    tft.setTextColor(TH().text, fill);
    tft.setCursor(x+55, y+16);
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
  if(usbInputHit(60,120,180,50)){resetSel=0;bOk.pressedEdge=true;}
  if(usbInputHit(240,120,180,50)){resetSel=1;bOk.pressedEdge=true;}
  if (bLeft.pressed() || bRight.pressed()) {
    resetSel ^= 1;
    drawResetScreen();
  }
  if (bOk.pressed() && resetSel==1) {
    settingsFactoryReset();
    settingsSave();
    drawResetScreen();
    tft.setTextSize(2);
    tft.setTextColor(TH().accent, TH().bg);
    tft.setCursor(20, 200);
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
  tft.fillRect(36,222,440,62,TH().bg);
  tft.setTextSize(1);tft.setTextColor(TH().dim,TH().bg);
  AudioDiagnostics d=audioGetDiagnostics();
  tft.setCursor(40,226);tft.printf("UART1 TX18 RX17 9600 | TX:%lu RX:%lu",(unsigned long)d.txPackets,(unsigned long)d.rxPackets);
  tft.setCursor(40,244);
  if(!d.ready)tft.print("Initializing DFPlayer / SD card...");
  else if(d.replySeen)tft.printf("Module replied | SD:%s | files:%u",d.cardKnown?(d.cardPresent?"OK":"absent"):"unknown",d.fileCount);
  else tft.print("No RX reply yet; test playback and check TX/GND.");
  tft.setCursor(40,262);tft.printf("Last MP3:%04u | error:%u | TX failures:%lu",d.lastTrack,d.lastError,(unsigned long)d.txFailures);
}

void setSoundEnter() {
  tft.fillScreen(TH().bg);
  drawHeader("Settings: Sound");
  drawFooter("L/R Volume  OK Mute", "BACK Save+Menu");
  clearBody();

  tft.setTextSize(2);
  tft.setTextColor(TH().text, TH().bg);
  tft.setCursor(30, 80);
  tft.print("Sound: ");
  tft.setTextColor(TH().accent, TH().bg);
  tft.print(soundEnabled ? "ON" : "OFF");

  // volume bar
  int x=40,y=140,w=400,h=26;
  tft.fillRoundRect(x, y, w, h, 10, TH().panel);
  tft.drawRoundRect(x, y, w, h, 10, TH().dim);
  int fillW = (soundVolume * (w-8)) / 255;
  tft.fillRoundRect(x+4, y+4, fillW, h-8, 8, TH().accent);

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(40, 180);
  tft.printf("Volume: %d", soundVolume);
  tft.setCursor(40, 202);tft.print("UP: Test MP3 | DOWN: UART diagnostics");
  drawSoundDiagnostics();
}

void setSoundLoop(uint32_t now) {
  static uint32_t lastStatus=0;
  if(bUp.pressed())audioTest();
  if(bDown.pressed())audioPrintDiagnostics();
  bool redraw = false;
  if(usbInputHit(30,72,420,32))bOk.pressedEdge=true;
  if(usbInputHit(40,140,400,26)){soundVolume=constrain((usbPointer.x-40)*255/399,0,255);redraw=true;}
  if(usbInputHit(40,194,400,22))bUp.pressedEdge=true;
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
 const char* names[]={"ILI9488 LCD","Composite TV (4:3)","TV alignment","AV colors",usbInputViewerMode()?"USB PC Viewer: ON":"USB PC Viewer: OFF"};
 tft.setTextSize(2);
 for(int i=0;i<5;i++){
  int y=58+i*31;tft.fillRoundRect(20,y,440,28,8,i==displayChoice?TH().selbg:TH().panel);
  tft.setTextColor(TH().text,i==displayChoice?TH().selbg:TH().panel);tft.setCursor(34,y+9);
  tft.print(i==displayChoice?"> ":"  ");tft.print(names[i]);
 }
 tft.setTextSize(1);tft.setTextColor(TH().dim,TH().bg);
 tft.setCursor(24,224);tft.print("BACK+RESET: LCD. AV D7..0: 48/39/40/42/2/14/13/8");
 tft.fillRoundRect(20,236,440,34,6,TH().panel);tft.setCursor(32,248);
 tft.print(displayConfirm?"Restart and apply? OK confirms.":"OK Apply / Open selected display settings");
 if(displaySaveError){tft.setCursor(24,279);tft.print("Could not save. Try again; output has not changed.");}
}
void setDisplayEnter(){displayChoice=freslest_video_output();displayConfirm=false;displaySaveError=false;drawDisplayPage();}
void setDisplayLoop(uint32_t now){
 (void)now;
 if(usbInputHit(20,58,440,155)){
  displayChoice=(usbPointer.y-58)/31;if(displayChoice>4)displayChoice=4;
  displayConfirm=false;displaySaveError=false;
  if((displayChoice==2||displayChoice==3)){enterApp(displayChoice==2?APP_SET_TV:APP_SET_TV_COLOR);return;}
  drawDisplayPage();return;
 }
 if(bUp.pressed()||bDown.pressed()||bLeft.pressed()||bRight.pressed()){
  int step=(bUp.pressed()||bLeft.pressed())?-1:1;
  displayChoice=(displayChoice+step+5)%5;displayConfirm=false;displaySaveError=false;drawDisplayPage();return;
 }
 if(usbInputHit(20,236,440,34))bOk.pressedEdge=true;
 if(!bOk.pressed())return;
 if((displayChoice==2||displayChoice==3)){enterApp(displayChoice==2?APP_SET_TV:APP_SET_TV_COLOR);return;}
 if(displayChoice<2&&displayChoice==freslest_video_output()){enterApp(APP_SETTINGS);return;}
 if(!displayConfirm){displayConfirm=true;drawDisplayPage();return;}
 if(displayChoice==4){if(!usbInputSwitchViewer(!usbInputViewerMode())){displaySaveError=true;drawDisplayPage();}return;}
 if(!freslest_video_save_output((freslest_display_t)displayChoice)){displaySaveError=true;drawDisplayPage();return;}
 audioStop();tft.fillScreen(0);drawHeader("Restarting display...");gfxPresent();delay(300);ESP.restart();
}

// Preview changes immediately; leaving with BACK or switching tasks restores
// the entry snapshot. Only OK Save commits the small configuration to NVS.
static freslest_tv_config_t tvOriginal,tvEdit;
static int tvRow=0;
static bool tvSaveError=false;
static void drawTvPage(){
 tft.fillScreen(TH().bg);drawHeader("Settings: TV alignment");clearBody();
 drawFooter("U/D Row L/R Adjust OK Save","BACK Cancel");
 const char *names[]={"Horizontal","Vertical","Size","Reset to default"};
 for(int i=0;i<4;i++){
  int y=58+i*38;int bg=i==tvRow?TH().selbg:TH().panel;
  tft.fillRoundRect(18,y,444,34,6,bg);tft.setTextSize(2);tft.setTextColor(TH().text,bg);
  tft.setCursor(28,y+9);tft.print(names[i]);
  if(i<3){tft.setCursor(224,y+9);if(i==2)tft.printf("%u%%",tvEdit.scale);else tft.printf("%+d",i==0?tvEdit.x:tvEdit.y);
   tft.setCursor(330,y+9);tft.print("-    +");}
 }
 tft.setTextSize(1);tft.setTextColor(tvSaveError?TFT_RED:TH().dim,TH().bg);
 tft.setCursor(24,220);tft.print(tvSaveError?"Save failed. BACK restores the previous settings.":"Reduce Size first to make room for moving the picture.");
 tft.fillRoundRect(18,238,444,36,6,TH().panel);tft.setTextColor(TH().text,TH().panel);
 tft.setTextSize(2);tft.setCursor(166,248);tft.print("OK: SAVE");
}
void setTvEnter(){tvOriginal=tvEdit=freslest_video_tv_config();tvRow=0;tvSaveError=false;drawTvPage();}
void setTvExit(){freslest_video_tv_preview(tvOriginal);}
void setTvLoop(uint32_t now){
 (void)now;int change=0;bool reset=false;
 bool clicked=usbInputHit(18,58,444,148);
 if(clicked){
  tvRow=(usbPointer.y-58)/38;if(tvRow>3)tvRow=3;
  if(tvRow==3)reset=true;else if(usbPointer.x>=304)change=usbPointer.x<368?-1:1;
 }
 bool up=bUp.pressed(),down=bDown.pressed();
 if(up)tvRow=(tvRow+3)%4;
 if(down)tvRow=(tvRow+1)%4;
 if(bLeft.pressed())change=-1;
 if(bRight.pressed())change=1;
 bool save=bOk.pressed()||usbInputHit(18,238,444,36);
 if(tvRow==3&&save){reset=true;save=false;}
 if(reset)tvEdit={0,0,100};
 else if(change){
  if(tvRow==0)tvEdit.x+=change;
  if(tvRow==1)tvEdit.y+=change;
  if(tvRow==2)tvEdit.scale+=change*2;
 }
 freslest_video_tv_preview(tvEdit);tvEdit=freslest_video_tv_config();
 if(save){
  if(freslest_video_tv_save(tvEdit)){tvOriginal=tvEdit;enterApp(APP_SET_DISPLAY);return;}
  tvSaveError=true;
 }
 // Redraw only on input; no continuous repaint while the pointer is idle.
 if(change||reset||save||up||down||clicked)drawTvPage();
}

// ===== AV colour: saved independently of picture size/position =====
static freslest_tv_color_t colorOriginal,colorEdit;
static int colorRow=0;
static bool colorSaveError=false;
static void drawTvColorPage(){
 tft.fillScreen(TH().bg);drawHeader("Settings: AV colors");clearBody();
 drawFooter("U/D Row L/R Adjust OK Save","BACK Cancel");
 const char *names[]={"Hue","Color","Detail","Reset to default"};
 for(int i=0;i<4;i++){
  int y=52+i*30;int bg=i==colorRow?TH().selbg:TH().panel;
  tft.fillRoundRect(18,y,444,27,5,bg);tft.setTextSize(2);tft.setTextColor(TH().text,bg);
  tft.setCursor(28,y+6);tft.print(names[i]);
  if(i<3){tft.setCursor(220,y+6);if(i==0)tft.printf("%+d",colorEdit.hue);else if(i==1)tft.printf("%u%%",colorEdit.saturation);else tft.print(colorEdit.detail==FRESLEST_TV_SHARP?"Sharp":"Clean");
   tft.setCursor(330,y+6);tft.print("-    +");}
 }
 // 75% bars: white, yellow, cyan, green, magenta, red, blue, black.
 const unsigned mask[]={7,6,3,2,5,4,1,0};const char *labels[]={"W","Y","C","G","M","R","B","K"};
 for(int i=0;i<8;i++){
  unsigned m=mask[i],v=((m&4)?23u<<11:0)|((m&2)?47u<<5:0)|((m&1)?23u:0);
  tft.fillRect(20+i*55,178,55,36,v);tft.setTextSize(1);tft.setTextColor(TH().text,TH().bg);tft.setCursor(44+i*55,220);tft.print(labels[i]);
  unsigned q=i*31/7,g=i*63/7;uint16_t grey=(q<<11)|(g<<5)|q;tft.fillRect(20+i*55,234,55,10,grey);
 }
 tft.setTextSize(1);tft.setTextColor(colorSaveError?TFT_RED:TH().dim,TH().bg);tft.setCursor(24,250);
 tft.print(colorSaveError?"Save failed. BACK restores previous colors.":colorEdit.saturation?"AV Engine 0.1.1. Color 0: black & white test.":"Black & white test. FIRE restores color defaults.");
 tft.fillRoundRect(18,264,444,28,5,TH().panel);tft.setTextColor(TH().text,TH().panel);tft.setTextSize(2);tft.setCursor(166,270);tft.print("OK: SAVE");
}
void setTvColorEnter(){colorOriginal=colorEdit=freslest_video_color_config();colorRow=0;colorSaveError=false;drawTvColorPage();}
void setTvColorExit(){freslest_video_color_preview(colorOriginal);}
void setTvColorLoop(uint32_t now){
 (void)now;int change=0;bool reset=bFire.pressed();
 bool clicked=usbInputHit(18,52,444,117);
 if(clicked){colorRow=(usbPointer.y-52)/30;if(colorRow>3)colorRow=3;
  if(colorRow==3)reset=true;else if(usbPointer.x>=304)change=usbPointer.x<368?-1:1;}
 bool up=bUp.pressed(),down=bDown.pressed();
 if(up)colorRow=(colorRow+3)%4;
 if(down)colorRow=(colorRow+1)%4;
 if(bLeft.pressed())change=-1;
 if(bRight.pressed())change=1;
 bool save=bOk.pressed()||usbInputHit(18,264,444,28);
 if(colorRow==3&&save){reset=true;save=false;}
 if(reset)colorEdit={0,100,0};
 else if(change){
  if(colorRow==0){int v=colorEdit.hue+change*5;colorEdit.hue=v < -180?-180:v>180?180:v;}
  if(colorRow==1){int v=colorEdit.saturation+change*5;colorEdit.saturation=v<0?0:v>160?160:v;}
  if(colorRow==2)colorEdit.detail=change>0?FRESLEST_TV_SHARP:FRESLEST_TV_CLEAN;
 }
 freslest_video_color_preview(colorEdit);colorEdit=freslest_video_color_config();
 if(save){if(freslest_video_color_save(colorEdit)){colorOriginal=colorEdit;enterApp(APP_SET_DISPLAY);return;}colorSaveError=true;}
 if(change||reset||save||up||down||clicked)drawTvColorPage();
}
