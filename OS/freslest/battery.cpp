#include "battery.h"
#include "gfx.h"
#include "src/freslest/settings.h"
#include "src/freslest/battery_status.h"
static uint32_t lastRead=0;static int percent=-1,shown=-2;static bool sampled=false;
float readBatteryVoltage() {
  uint32_t mv=0;for(int i=0;i<4;i++)mv+=analogReadMilliVolts(ADC_BAT);
  return (mv/4000.0f)*(147.0f/47.0f);
}
int batteryPercent(float v) {
  return batteryPercentForVoltage(v);
}
void drawBatteryTopBar(bool force) {
  uint32_t n=millis();if(!sampled||n-lastRead>=2000) {percent=batteryPercent(readBatteryVoltage());lastRead=n;sampled=true;}
  if(!force&&shown==percent)return;
  tft.fillRect(400,2,34,36,TH().panel);tft.setTextSize(1);tft.setTextColor(TH().text,TH().panel);tft.setCursor(401,3);
  if(percent<0) {tft.setTextColor(TFT_RED,TH().panel);tft.print("X");}else tft.printf("%d%%",percent);
  tft.drawRect(401,18,28,12,TH().text);tft.fillRect(429,22,2,4,TH().text);
  if(percent<0) {
    tft.drawLine(405,20,424,27,TFT_RED);tft.drawLine(405,27,424,20,TFT_RED);
  } else {
    uint16_t color=percent<=10?TFT_RED:percent<=40?TFT_YELLOW:TFT_GREEN;
    tft.fillRect(402,19,percent*26/100,10,color);
  }
  shown=percent;
}
