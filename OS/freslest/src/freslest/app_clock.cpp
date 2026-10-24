#include "apps.h"
#include "../../gfx.h"
#include "../../input.h"
#include "system_status.h"
#include "settings.h"
#include <WiFi.h>
#include "../../usb_input.h"
static uint32_t lastDraw=0;
static bool editing=false;
static int field=0,values[6]={2026,1,1,0,0,0};
static const char* labels[]={"Year","Month","Day","Hour","Minute","Second"};
static const int lo[]={2024,1,1,0,0,0},hi[]={2099,12,31,23,59,59};
static void drawClock() {
  tft.fillScreen(TH().bg);drawHeader("Clock");clearBody();
  drawFooter(editing?"L/R Field U/D Value":"OK Set Clock",editing?"OK Save BACK Cancel":"UP NTP  BACK Apps");
  struct tm t;bool valid=rtcGetLocal(t);
  tft.setTextColor(TH().text,TH().bg);tft.setTextSize(3);tft.setCursor(28,64);
  char text[40];
  if(valid) {strftime(text,sizeof(text),"%H:%M:%S  %d.%m.%Y",&t);tft.setTextSize(2);tft.print(text);}
  else {tft.print("Clock not set");}
  if(editing) {
    for(int i=0;i<6;i++) {
      int x=12+(i%3)*156,y=100+(i/3)*64;
      tft.fillRoundRect(x,y,148,56,6,i==field?TH().selbg:TH().panel);
      tft.drawRoundRect(x,y,148,56,6,i==field?TH().accent:TH().dim);
      tft.setTextSize(1);tft.setTextColor(TH().text,i==field?TH().selbg:TH().panel);tft.setCursor(x+10,y+7);tft.print(labels[i]);
      tft.setTextSize(2);tft.setCursor(x+10,y+28);tft.printf("%04d",values[i]);
    }
    tft.setTextSize(1);tft.setTextColor(TH().text,TH().bg);
    tft.setCursor(24,238);tft.print("LEFT/RIGHT: Field | UP/DOWN: Value");
    tft.setCursor(24,264);tft.print("OK: Save | BACK: Cancel");
  } else {
    tft.setTextSize(1);tft.setTextColor(TH().dim,TH().bg);tft.setCursor(28,116);tft.print("ESP32-S3 internal RTC | Turkey UTC+3");
    tft.setCursor(28,140);tft.print("NTP sets the clock when WiFi connects.");
    tft.setCursor(28,162);tft.print("Press OK to set date and time manually.");
    tft.setCursor(28,184);tft.print("Set the clock again after power loss.");
    tft.setCursor(28,218);tft.print("OK: Set date and time");
    tft.setCursor(28,240);tft.print("UP: Refresh NTP");
  }
}
void clockEnter() {editing=false;lastDraw=millis();drawClock();}
bool clockCancelEdit() {if(!editing)return false;editing=false;drawClock();return true;}
static void beginEdit() {
  struct tm t;
  if(rtcGetLocal(t)) {values[0]=t.tm_year+1900;values[1]=t.tm_mon+1;values[2]=t.tm_mday;values[3]=t.tm_hour;values[4]=t.tm_min;values[5]=t.tm_sec;}
  editing=true;field=0;drawClock();
}
void clockLoop(uint32_t now) {
  if(!editing) {
    if(usbInputHit(28,206,300,24))bOk.pressedEdge=true;
    if(usbInputHit(28,232,300,24))bUp.pressedEdge=true;
    if(bOk.pressed()) {beginEdit();return;}
    if(bUp.pressed())rtcSyncRequest();
    if(now-lastDraw>=1000) {lastDraw=now;drawClock();}
    return;
  }
  bool redraw=false,save=bOk.pressed();int delta=0;
  for(int i=0;i<6;i++)if(usbInputHit(12+(i%3)*156,100+(i/3)*64,148,56)){field=i;redraw=true;}
  if(usbInputHit(24,254,160,24))save=true;
  if(bLeft.pressed()) {field=(field+5)%6;redraw=true;} if(bRight.pressed()) {field=(field+1)%6;redraw=true;}
  if(bUp.pressed())delta=1;
  if(bDown.pressed())delta=-1;
  if(delta) {values[field]+=delta;
  if(values[field]>hi[field])values[field]=lo[field];
  if(values[field]<lo[field]) {values[field]=hi[field];}
    redraw=true;
  }
  if(save) {
    if(rtcSetLocal(values[0],values[1],values[2],values[3],values[4],values[5])) {editing=false;drawClock();return;}
    tft.setTextColor(TFT_RED,TH().bg);tft.setTextSize(1);tft.setCursor(24,236);tft.print("Invalid date: check day and month.");return;
  }
  if(redraw)drawClock();
}
