#include "system_status.h"
#include "dht11_decode.h"
#include "settings.h"
#include "wifi_manager.h"
#include "ble_advertising.h"
#include "../../gfx.h"
#include <WiFi.h>
#include <sys/time.h>
#include <driver/gpio.h>
#include <esp_timer.h>

static constexpr int DHT_PIN=38;
static bool dhtStarted=false,dhtValid=false,ntpStarted=false;
static uint32_t dhtStart=0,dhtLast=0,dhtGood=0;
static float temperature=0,humidity=0;
static portMUX_TYPE dhtMux=portMUX_INITIALIZER_UNLOCKED;
static char previous[48]="";
static uint32_t statusDrawAt=0;
static uint32_t ntpAttemptAt=0;
static bool hadInternetLink=false;
static bool waitLevel(int level,uint32_t limit) {
  int64_t start=esp_timer_get_time();
  while(gpio_get_level((gpio_num_t)DHT_PIN)==level)
    if(esp_timer_get_time()-start>limit)return false;
  return true;
}
static bool readDht() {
  uint8_t data[5]={0};bool ok=true;
  portENTER_CRITICAL(&dhtMux);
  gpio_set_direction((gpio_num_t)DHT_PIN,GPIO_MODE_INPUT);
  // end host high pulse -> sensor response LOW/HIGH -> first data LOW
  ok=waitLevel(1,120)&&waitLevel(0,120)&&waitLevel(1,120);
  for(int i=0;ok&&i<40;++i) {
    ok=waitLevel(0,100);
    int64_t start=esp_timer_get_time();
    if(ok)ok=waitLevel(1,100);
    if(ok) data[i/8]=(uint8_t)((data[i/8]<<1)|((esp_timer_get_time()-start)>45));
  }
  portEXIT_CRITICAL(&dhtMux);
  return ok&&dht11Decode(data,temperature,humidity);
}
bool rtcGetLocal(struct tm& out) {
  time_t now=time(nullptr);
  if(now<1704067200LL||now>4102444799LL)return false;
  return localtime_r(&now,&out)!=nullptr;
}
bool rtcSetLocal(int year,int month,int day,int hour,int minute,int second) {
  if(year<2024||year>2099||month<1||month>12||day<1||day>31||hour<0||hour>23||minute<0||minute>59||second<0||second>59)return false;
  struct tm t={};t.tm_year=year-1900;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;t.tm_sec=second;t.tm_isdst=-1;
  time_t epoch=mktime(&t);
  if(t.tm_year!=year-1900||t.tm_mon!=month-1||t.tm_mday!=day)return false;
  timeval tv={epoch,0};return settimeofday(&tv,nullptr)==0;
}
void rtcSyncRequest() {
  configTzTime("TRT-3","pool.ntp.org","time.nist.gov");ntpStarted=true;ntpAttemptAt=millis();
}
void systemStatusBegin() {
  setenv("TZ","TRT-3",1);tzset();
  pinMode(DHT_PIN,INPUT_PULLUP);dhtLast=millis();
  wifiManagerBegin();
}
void systemStatusUpdate(uint32_t now) {
  wifiManagerUpdate(now);
  bool online=WiFi.status()==WL_CONNECTED;struct tm clock;
  if(online&&(!hadInternetLink||!ntpStarted||(!rtcGetLocal(clock)&&(uint32_t)(now-ntpAttemptAt)>=30000u)))rtcSyncRequest();
  hadInternetLink=online;
  if(!dhtStarted&&now-dhtLast>=2100) {
    pinMode(DHT_PIN,OUTPUT_OPEN_DRAIN);digitalWrite(DHT_PIN,LOW);
    dhtStart=now;dhtStarted=true;
  } else if(dhtStarted&&now-dhtStart>=20) {
    digitalWrite(DHT_PIN,HIGH);delayMicroseconds(30);
    dhtValid=readDht();if(dhtValid)dhtGood=now;
    dhtStarted=false;dhtLast=now;
  }
}
bool dhtReading(float& t,float& h) {
  if(!dhtValid||millis()-dhtGood>6000)return false;
  t=temperature;h=humidity;return true;
}
void systemStatusDraw(bool force) {
  uint32_t now=millis();if(!force&&now-statusDrawAt<250)return;statusDrawAt=now;
  struct tm t;char clock[16]="--:--:--",temp[16]="--.- C",text[80];float c,h;
  if(rtcGetLocal(t))strftime(clock,sizeof(clock),"%H:%M:%S",&t);
  if(dhtReading(c,h))snprintf(temp,sizeof(temp),"%.1f C",c);
  auto bt=freslestBleInfo();
  bool connected=WiFi.status()==WL_CONNECTED;
  unsigned wifiState=connected?2:(wifiEnabled||wifiPortalActive())?1:0,btState=bt.connected?2:bt.enabled?1:0;
  snprintf(text,sizeof(text),"%s|%s|%u|%u",clock,temp,wifiState,btState);
  if(!force&&!strcmp(text,previous))return;
  strlcpy(previous,text,sizeof(previous));
  bool compact=tft.width()==320;
  tft.fillRect(compact?110:116,2,compact?164:280,20,TH().panel);
  tft.setTextSize(1);tft.setTextColor(TH().text,TH().panel);
  tft.setCursor(compact?112:120,8);if(compact)tft.printf("%.5s",clock);else tft.print(clock);tft.setCursor(compact?162:180,8);tft.print(temp);
  uint16_t wc=wifiState==2?TH().accent:wifiState==1?0xFDA0:TH().dim;
  const int x=compact?225:354,y=6;
  tft.drawLine(x-9,y+3,x-5,y,wc);tft.drawLine(x-5,y,x+5,y,wc);tft.drawLine(x+5,y,x+9,y+3,wc);
  tft.drawLine(x-6,y+7,x-3,y+4,wc);tft.drawLine(x-3,y+4,x+3,y+4,wc);tft.drawLine(x+3,y+4,x+6,y+7,wc);
  tft.fillCircle(x,y+12,2,wc);
  uint16_t bc=btState==2?TH().accent:btState==1?0xFDA0:TH().dim;const int bx=compact?255:382,by=4;
  tft.drawLine(bx,by,bx,by+17,bc);tft.drawLine(bx,by,bx+5,by+5,bc);
  tft.drawLine(bx+5,by+5,bx-5,by+13,bc);tft.drawLine(bx-5,by+4,bx+5,by+12,bc);
  tft.drawLine(bx+5,by+12,bx,by+17,bc);
}
