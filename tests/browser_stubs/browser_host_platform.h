#pragma once
#include "../ble_stubs/ble_host_platform.h"
#include <string>
using TaskHandle_t=void*;
constexpr int WL_CONNECTED=3;
inline void *heap_caps_malloc(size_t n,int){return malloc(n);}
struct WifiMock{int status(){return WL_CONNECTED;}};inline WifiMock WiFi;
struct NetworkClient {
 std::string incoming,written;size_t pos=0;bool online=true;unsigned chunk=113;
 bool connected(){return online;}bool connect(const char*,int,int){online=true;return true;}
 void setTimeout(int){}void setNoDelay(bool){}void stop(){online=false;}
 int available(){return (int)std::min((size_t)chunk,incoming.size()-pos);}
 int read(){return pos<incoming.size()?(uint8_t)incoming[pos++]:-1;}
 int read(uint8_t*out,size_t n){n=std::min(n,incoming.size()-pos);memcpy(out,incoming.data()+pos,n);pos+=n;return n;}
 size_t write(const uint8_t*p,size_t n){assert(!critical);written.append((const char*)p,n);return n;}
};
