#include "screen_mirror.h"
#include "screen_mirror_codec.h"
#ifdef FRESLEST_SCREEN_HOST_TEST
#include "screen_host_platform.h"
#else
#include "usb_input.h"
#include <Arduino.h>
#include <USB.h>
#include <tusb.h>
#include <esp32-hal-tinyusb.h>
#include <esp_heap_caps.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include <atomic>
#include <cstdio>
#include <cstdlib>
namespace {
TaskHandle_t writer=nullptr;
uint8_t*packet=nullptr;
size_t packetLength=0;
std::atomic<bool> busy{false};
bool active=false,wanted=false,stopping=false,nativeStarted=false;
unsigned remote=0;
uint32_t lastRequest=0,lastControl=0,sequence=0;
unsigned divisor=1,diagnostics=0,owner=0,width=480,height=320;
char command[2][96];unsigned used[2]{};bool overflow[2]{};
#ifndef FRESLEST_SCREEN_HOST_TEST
uint16_t descriptor(uint8_t*dst,uint8_t*itf){
 uint8_t str=tinyusb_add_string_descriptor("Freslest PC Viewer");
 const uint8_t d[]={TUD_CDC_DESCRIPTOR(*itf,str,0x85,64,0x03,0x84,64)};
 *itf+=2;memcpy(dst,d,sizeof(d));return sizeof(d);
}
#endif
bool connected(unsigned port){return !port||tud_cdc_connected();}
void sendText(unsigned port,const char*s){
 if(!port){Serial.print(s);}
 else if(tud_cdc_connected()){tud_cdc_write(s,strlen(s));tud_cdc_write_flush();}
}
void releaseRemote(){
 if(remote){usbInputPcRelease();remote=0;}
}
void transmit(void*){
 for(;;){
  ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
  if(!owner){
   // IDF TX keeps the Arduino RX mutex free while a frame is being sent.
   uart_write_bytes(UART_NUM_0,packet,packetLength);
   uart_wait_tx_done(UART_NUM_0,pdMS_TO_TICKS(1000));
  }else{
   size_t offset=0;uint32_t progress=millis();
   while(offset<packetLength&&tud_cdc_connected()){
    size_t n=tud_cdc_write_available();if(n>packetLength-offset)n=packetLength-offset;
    if(n){size_t sent=tud_cdc_write(packet+offset,n);offset+=sent;if(sent)progress=millis();}
    tud_cdc_write_flush();
    if((uint32_t)(millis()-progress)>1500u)break;
    vTaskDelay(1);
   }
  }
  busy.store(false,std::memory_order_release);
 }
}
int hexDigit(char c){
 if(c>='0'&&c<='9')return c-'0';
 if(c>='a'&&c<='f')return c-'a'+10;
 if(c>='A'&&c<='F')return c-'A'+10;
 return -1;
}
void handle(unsigned port){
 char*s=command[port];
 if(!strcmp(s,"FS?")){
  if(active&&port==owner)return;
  char reply[64];snprintf(reply,sizeof(reply),"FSHELLO 2 %s %u %u\n",port?"NATIVE":"UART",width,height);sendText(port,reply);return;
 }
 if(active&&port!=owner){if(!strncmp(s,"FS1 ",4))sendText(port,"FSBUSY\n");return;}
 if(!strcmp(s,"FS0")){if(active)stopping=true;return;}
 if(!strcmp(s,"FS2")){if(active&&!stopping){wanted=true;lastRequest=millis();}return;}
 if(active&&!stopping){
  if(!strcmp(s,"FSR")){releaseRemote();return;}
  if(!strncmp(s,"FSK ",4)&&strlen(s)==68){
   uint8_t keys[32];
   for(unsigned i=0;i<32;++i){int a=hexDigit(s[4+i*2]),b=hexDigit(s[5+i*2]);if(a<0||b<0)return;keys[i]=(a<<4)|b;}
   remote|=1;usbInputPcKeys(keys);lastControl=millis();return;
  }
  int x,y,b;char extra;
  if(sscanf(s,"FSP %d %d %d %c",&x,&y,&b,&extra)==3&&x>=0&&y>=0&&(unsigned)x<width&&(unsigned)y<height&&b>=0&&b<=7){
   remote|=2;
   // Absolute positions must not accumulate deltas when commands queue.
   usbInputPcMouse(x,y,b);lastControl=millis();return;
  }
 }
 unsigned baud=0,scale=0;char extra;
 if(sscanf(s,"FS1 %u %u %c",&baud,&scale,&extra)!=2||active||busy.load())return;
 if((baud!=921600&&baud!=460800)||(scale!=1&&scale!=2))return;
 packet=(uint8_t*)heap_caps_malloc(freslest_screen::maxPacket,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!packet){sendText(port,"FSERROR PSRAM\n");return;}
 if(!writer&&xTaskCreatePinnedToCore(transmit,"usb_screen",4096,nullptr,1,&writer,1)!=pdPASS){free(packet);packet=nullptr;sendText(port,"FSERROR RAM\n");return;}
 owner=port;
 char reply[32];snprintf(reply,sizeof(reply),"FSREADY %u\n",baud);sendText(port,reply);
 if(!port){Serial.flush();Serial.updateBaudRate(baud);}
 active=true;wanted=false;stopping=false;divisor=scale;lastRequest=millis();
}
void consume(unsigned port){
 for(unsigned i=0;i<256&&(port?tud_cdc_available():Serial.available());++i){
  int c=port?tud_cdc_read_char():Serial.read();
  if(!used[port]&&!overflow[port]){
   if(c=='F')command[port][used[port]++]=c;
   else if(!port&&c=='m')diagnostics|=1;
   else if(!port&&c=='i')diagnostics|=2;
   continue;
  }
  if(c=='\n'){if(!overflow[port]){command[port][used[port]]=0;handle(port);}used[port]=0;overflow[port]=false;}
  else if(c!='\r'){if(used[port]<sizeof(command[port])-1)command[port][used[port]++]=c;else overflow[port]=true;}
 }
}
}
bool screenMirrorNativeBegin(){
 if(nativeStarted)return true;
#ifndef FRESLEST_SCREEN_HOST_TEST
 if(tinyusb_enable_interface(USB_INTERFACE_CDC,TUD_CDC_DESC_LEN,descriptor)!=ESP_OK)return false;
 USB.productName("Freslest PC Viewer");
 if(!USB.begin())return false;
#endif
 nativeStarted=true;return true;
}
void screenMirrorPoll(){
 consume(0);if(nativeStarted)consume(1);
 if(remote&&(uint32_t)(millis()-lastControl)>900u)releaseRemote();
 if(active&&((uint32_t)(millis()-lastRequest)>5000u||!connected(owner)))stopping=true;
 if(stopping&&!busy.load(std::memory_order_acquire)){
  releaseRemote();
  if(!owner){Serial.flush();Serial.updateBaudRate(115200);}
  used[owner]=0;overflow[owner]=false;active=wanted=stopping=false;
  free(packet);packet=nullptr;
 }
}
void screenMirrorFrame(const uint16_t*raw,unsigned w,unsigned h,bool cursor,int x,int y){
 width=w;height=h;
 if(!active||!wanted||stopping||busy.load(std::memory_order_acquire))return;
 packetLength=freslest_screen::encode(packet,freslest_screen::maxPacket,raw,w,h,divisor,true,cursor,x,y,++sequence);
 wanted=false;if(!packetLength)return;
 busy.store(true,std::memory_order_release);xTaskNotifyGive(writer);
}
bool screenMirrorActive(){return active;}
unsigned screenMirrorDiagnostics(){unsigned v=diagnostics;diagnostics=0;return v;}
