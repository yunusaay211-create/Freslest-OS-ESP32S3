#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
using esp_err_t=int;using nvs_handle_t=int;
constexpr int ESP_OK=0,NVS_READWRITE=0,APP_FILES=99,APP_BROWSER=100;inline int app=0;inline uint8_t savedPc=0,savedNes=0,savedScreen=0;inline bool nvsFail=false;
inline int nvs_open(const char*,int,int* h){*h=1;return nvsFail?-1:0;}inline int nvs_get_u8(int,const char* k,uint8_t* n){*n=!strcmp(k,"screen")?savedScreen:strcmp(k,"pc")?savedNes:savedPc;return 0;}inline int nvs_set_u8(int,const char* k,uint8_t n){(!strcmp(k,"screen")?savedScreen:strcmp(k,"pc")?savedNes:savedPc)=n;return 0;}inline int nvs_commit(int){return nvsFail?-1:0;}inline void nvs_close(int){}inline const char* esp_err_to_name(int){return "error";}
#ifdef FRESLEST_SCREEN_INPUT_INTEGRATION
#include "screen_host_platform.h"
#else
struct Debug{void println(const char*){}template<class... A>void printf(const char*,A...){}};inline Debug Serial;
#endif
struct Display{int heightValue=320;int width(){return 480;}int height(){return heightValue;}void fillScreen(int){}void setTextColor(int,int){}void setTextSize(int){}void setCursor(int,int){}void print(const char*){}};inline Display tft;
inline void drawHeader(const char*){}inline void clearBody(){}inline void audioStop(){}inline void gfxPresent(){}inline void delay(int){}
struct Restart{};struct Esp{void restart(){throw Restart{};}};inline Esp ESP;
template<class T>T constrain(T n,T lo,T hi){return std::min(hi,std::max(lo,n));}

#ifndef FRESLEST_SCREEN_INPUT_INTEGRATION
inline bool screenMirrorNativeBegin(){return true;}
#endif
