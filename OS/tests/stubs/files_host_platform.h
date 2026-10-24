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
struct Display{std::vector<std::string> words;void fillScreen(unsigned){words.clear();}void fillRoundRect(int,int y,int,int h,int,unsigned){assert(y>=44&&y+h<=296);}void drawRoundRect(int,int y,int,int h,int,unsigned){assert(y>=44&&y+h<=296);}void setTextSize(unsigned){}void setTextColor(unsigned,unsigned){}void setCursor(int x,int y){assert(x>=0&&x<480&&y>=44&&y<296);}void print(const char*s){words.emplace_back(s);}};inline Display tft;
inline void drawHeader(const char*){}inline void clearBody(){}inline void drawFooter(const char*,const char*){}
constexpr unsigned TFT_RED=0xf800;constexpr int APP_APPS=1;inline int entered=-1;inline void enterApp(int app){entered=app;}
struct FS{size_t totalBytes(){return 320*1024;}size_t usedBytes(){return 24000;}};inline FS LittleFS;

inline bool usbInputHostMode(){return !hostPC;}
