#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
#include "../../freslest/usb_input.h"
#include "../../freslest/src/freslest/usb_storage.h"
struct Button{bool pressedEdge=false;bool pressed()const{return pressedEdge;}};inline Button bBack,bUp,bDown,bLeft,bRight,bOk,bFire;inline UsbPointer usbPointer;
inline bool usbInputHit(int x,int y,int w,int h){return usbPointer.visible&&usbPointer.click&&usbPointer.x>=x&&usbPointer.x<x+w&&usbPointer.y>=y&&usbPointer.y<y+h;}
inline bool freslestStorageEnsure(){return true;}inline const char* freslestStorageStatus(){return "Ready";}
inline UsbStorageInfo hostUsb;
inline const char *hostUsbRoot="/usb";
inline unsigned usbLocks=0,ejects=0,retries=0,modeSwitches=0;
inline bool hostPC=false;
inline unsigned storageBegins=0;inline bool usbStorageBegin(){++storageBegins;return true;}
inline UsbStorageInfo usbStorageInfo(){return hostUsb;}
inline const char *usbStorageRoot(){return hostUsbRoot;}
inline bool usbStorageLock(){if(hostUsb.state!=UsbStorageState::Ready)return false;usbLocks++;return true;}
inline void usbStorageUnlock(){assert(usbLocks);usbLocks--;}
inline bool usbStorageEject(){ejects++;hostUsb.state=UsbStorageState::Ejected;hostUsb.revision++;return true;}
inline bool usbStorageRetry(){retries++;return true;}
inline bool usbInputDeviceMode(){return hostPC;}
inline bool usbInputSwitchMode(bool pc){hostPC=pc;modeSwitches++;return true;}
struct Theme{unsigned bg=0,panel=1,text=2,dim=3,selbg=4,accent=5;};inline Theme theme;inline Theme& TH(){return theme;}
struct Display{
#ifdef FRESLEST_TEST_NATIVE_UI
 int w=320;
#else
 int w=480;
#endif
 int x=0,y=0,size=1;std::vector<std::string> words;int width(){return w;}int height(){return w==320?240:320;}
 void bounds(int a,int b,int ww,int hh){assert(a>=0&&a+ww<=w&&b>=(w==320?36:44)&&b+hh<=height()-24);}
 void fillScreen(unsigned){words.clear();}void fillRect(int x,int y,int w,int h,unsigned){bounds(x,y,w,h);}
 void fillRoundRect(int x,int y,int w,int h,int,unsigned c){fillRect(x,y,w,h,c);}void drawRoundRect(int x,int y,int w,int h,int,unsigned c){fillRect(x,y,w,h,c);}
 void drawRect(int x,int y,int w,int h,unsigned c){fillRect(x,y,w,h,c);}void drawLine(int x,int y,int xx,int yy,unsigned c){fillRect(std::min(x,xx),std::min(y,yy),abs(xx-x)+1,abs(yy-y)+1,c);}
 void setTextSize(unsigned n){size=n;}void setTextColor(unsigned,unsigned){}void setCursor(int a,int b){x=a;y=b;bounds(x,y,0,size*8);}
 void print(const char*s){assert(x+(int)strlen(s)*6*size<=w);words.emplace_back(s);}
};inline Display tft;
inline void drawHeader(const char*){}inline void clearBody(){}inline void drawFooter(const char*,const char*){}
constexpr unsigned TFT_RED=0xf800;constexpr int APP_APPS=1;inline int entered=-1;inline void enterApp(int app){entered=app;}
struct FS{size_t totalBytes(){return 320*1024;}size_t usedBytes(){return 24000;}};inline FS LittleFS;

inline bool usbInputHostMode(){return !hostPC;}
