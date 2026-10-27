#pragma once
#include "browser_native.h"
#include "text_entry.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <string>
#include <vector>
enum {APP_APPS=1,APP_SET_WIFI=2};inline int enteredApp=-1;
inline void enterApp(int n){enteredApp=n;}
inline size_t strlcpy(char*d,const char*s,size_t n){size_t len=strlen(s);if(n){memmove(d,s,std::min(n-1,len));d[std::min(n-1,len)]=0;}return len;}
struct ButtonMock{bool edge=false;bool pressed(){return edge;}};inline ButtonMock bUp,bDown,bLeft,bRight,bBack,bOk,bFire;
struct PointerMock{int x=0,y=0;bool click=false;}inline usbPointer;
inline bool usbInputHit(int x,int y,int w,int h){return usbPointer.click&&usbPointer.x>=x&&usbPointer.x<x+w&&usbPointer.y>=y&&usbPointer.y<y+h;}
inline void usbInputTextMode(bool){}
struct ThemeMock{unsigned bg=0,panel=1,dim=2,text=3,accent=4,selbg=5;};inline ThemeMock theme;
inline ThemeMock&TH(){return theme;}
struct DisplayMock{
 
#ifdef FRESLEST_TEST_NATIVE_UI
 int w=320,h=240;
#else
 int w=480,h=320;
#endif
 int cursorX=0,cursorY=0,size=1;std::vector<std::string> words;
 int width(){return w;}int height(){return h;}void fillScreen(unsigned){words.clear();}
 void fillRect(int x,int y,int w,int h1,unsigned){assert(x>=0&&x+w<=this->w&&y>=(this->w==320?36:44)&&y+h1<=h-24);}
 void fillRoundRect(int x,int y,int w,int h1,int,unsigned c){fillRect(x,y,w,h1,c);}void drawRect(int x,int y,int w,int h1,unsigned c){fillRect(x,y,w,h1,c);}
 void drawRoundRect(int x,int y,int w,int h1,int,unsigned c){fillRect(x,y,w,h1,c);}
 void drawLine(int x,int y,int xx,int yy,unsigned c){fillRect(std::min(x,xx),std::min(y,yy),abs(xx-x)+1,abs(yy-y)+1,c);}
 void setTextSize(unsigned n){size=n;}void setTextColor(unsigned,unsigned){}
 void setCursor(int x,int y){cursorX=x;cursorY=y;assert(x>=0&&y>=(w==320?36:44)&&y+8*size<=h-24);}
 void print(const char*s){words.emplace_back(s);assert(cursorX+(int)strlen(s)*6*size<=w);}
 template<class...A>void printf(const char*f,A...a){char b[512];snprintf(b,sizeof(b),f,a...);print(b);}
 void pushImage(int x,int y,unsigned w,unsigned h1,uint16_t*){assert(x>=(this->w==320?12:18)&&x+(int)w<=this->w-(this->w==320?12:18)&&y>=(this->w==320?60:76)&&y+(int)h1<=h-48);}
};inline DisplayMock tft;
inline void drawHeader(const char*){}inline void drawFooter(const char*,const char*){}inline void clearBody(){}
inline bool editingNow=false;inline std::string editedValue;inline TextEntryResult editorResult=TextEntryResult::None;
inline void textEntryBegin(const char*,const char*value,unsigned,bool){editingNow=true;editedValue=value;}
inline void textEntryCancel(){editingNow=false;}inline TextEntryResult textEntryTakeResult(char*out,size_t cap){auto r=editorResult;if(r!=TextEntryResult::None){strlcpy(out,editedValue.c_str(),cap);editorResult=TextEntryResult::None;editingNow=false;}return r;}
constexpr int JDR_OK=0;
struct JpegMock{unsigned scale=1;void setSwapBytes(bool){}template<class T>void setCallback(T){}int getJpgSize(uint16_t*w,uint16_t*h,const uint8_t*,size_t){*w=400;*h=224;return 0;}void setJpgScale(unsigned n){scale=n;}void drawJpg(int,int,const uint8_t*,size_t){}};inline JpegMock TJpgDec;
inline NativePage* nextPage=nullptr;inline std::vector<std::string> requests;inline unsigned cancelled=0,released=0;
inline bool nativeBrowserOpen(const char*s){requests.emplace_back(s);return true;}inline void nativeBrowserCancel(){cancelled++;}
inline NativePage*nativeBrowserTake(int&slot){auto*p=nextPage;nextPage=nullptr;slot=p?0:-1;return p;}inline void nativeBrowserRelease(int){released++;}
inline NativeBrowserStatus nativeBrowserStatus(){NativeBrowserStatus s{};strcpy(s.message,"Ready");return s;}
