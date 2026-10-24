#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <cstdarg>
extern "C" {
#include "freslest_hid_state.h"
}
using TaskHandle_t=void*;
constexpr int UART_NUM_0=0,MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2,pdPASS=1,pdTRUE=1,portMAX_DELAY=-1;
inline uint32_t fakeNow=100;inline bool mallocFail=false,taskFail=false,nativeConnected=true;inline unsigned notifications=0,allocationCaps=0;
inline uint32_t millis(){return fakeNow;}
inline void*heap_caps_malloc(size_t n,int caps){allocationCaps=caps;return mallocFail?nullptr:malloc(n);}
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,void**out,int){if(taskFail)return 0;*out=(void*)1;return pdPASS;}
inline int ulTaskNotifyTake(int,int){return 1;}
inline void xTaskNotifyGive(void*){++notifications;}
inline int pdMS_TO_TICKS(int n){return n;}
inline void vTaskDelay(int n){fakeNow+=n;}
inline int uart_write_bytes(int,const void*,size_t n){return n;}
inline void uart_wait_tx_done(int,int){}
struct Port{
 unsigned baud=115200;std::string rx,tx;unsigned flushes=0;
 int available(){return rx.size();}int read(){int v=(uint8_t)rx.front();rx.erase(0,1);return v;}
 void updateBaudRate(unsigned b){baud=b;}void flush(){++flushes;}
 void print(const char*s){tx+=s;}void println(const char*s){tx+=s;tx+='\n';}
 void printf(const char*fmt,...){char b[128];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);tx+=b;}
};inline Port Serial,Native;
inline bool tud_cdc_connected(){return nativeConnected;}
inline unsigned tud_cdc_available(){return Native.available();}
inline int tud_cdc_read_char(){return Native.read();}
inline unsigned tud_cdc_write_available(){return 1024;}
inline unsigned tud_cdc_write(const void*p,unsigned n){Native.tx.append((const char*)p,n);return n;}
inline void tud_cdc_write_flush(){}
#ifndef FRESLEST_SCREEN_INPUT_INTEGRATION
inline struct Pointer{int x=0,y=0;}usbPointer;
inline freslest_hid_state_t remoteState{};
inline void usbInputPcKeys(const uint8_t*k){if(!remoteState.slot[6].kind)freslest_hid_state_connect(&remoteState,6,1);freslest_hid_state_keys(&remoteState,6,k);}
inline void usbInputPcMouse(int x,int y,uint8_t b){if(!remoteState.slot[7].kind)freslest_hid_state_connect(&remoteState,7,2);usbPointer.x=x;usbPointer.y=y;freslest_hid_state_mouse(&remoteState,7,b,0,0);}
inline void usbInputPcRelease(){freslest_hid_state_disconnect(&remoteState,6);freslest_hid_state_disconnect(&remoteState,7);}
extern "C" {
inline void freslest_hid_external_connect(unsigned i,unsigned k){freslest_hid_state_connect(&remoteState,i+4,k);}
inline void freslest_hid_external_keys(unsigned i,const uint8_t*k){freslest_hid_state_keys(&remoteState,i+4,k);}
}
#endif
