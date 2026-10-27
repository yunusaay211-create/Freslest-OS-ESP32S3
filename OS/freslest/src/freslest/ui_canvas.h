#pragma once
// Legacy app geometry on a native canvas. Text glyphs are never resampled.
#include <algorithm>
#include <cstdio>
#include <cstring>
static inline bool uiCanvasCompact(){return tft.width()==320;}
static inline int uiCanvasX(int x){return uiCanvasCompact()?x*2/3:x;}
static inline int uiCanvasY(int y){return uiCanvasCompact()?36+(y-44)*5/7:y;}
static inline int uiCanvasHeight(){return uiCanvasCompact()?320:tft.height();}
static inline int uiCanvasPointerX(){return uiCanvasCompact()?(usbPointer.x*3+1)/2:usbPointer.x;}
static inline int uiCanvasPointerY(){return uiCanvasCompact()?44+((usbPointer.y-36)*7+4)/5:usbPointer.y;}
static inline bool uiCanvasHit(int x,int y,int w,int h){return usbInputHit(uiCanvasX(x),uiCanvasY(y),uiCanvasX(x+w)-uiCanvasX(x),uiCanvasY(y+h)-uiCanvasY(y));}
static inline void uiCanvasFillRect(int x,int y,int w,int h,unsigned c){tft.fillRect(uiCanvasX(x),uiCanvasY(y),uiCanvasX(x+w)-uiCanvasX(x),uiCanvasY(y+h)-uiCanvasY(y),c);}
static inline void uiCanvasRoundRect(int x,int y,int w,int h,int r,unsigned c){tft.fillRoundRect(uiCanvasX(x),uiCanvasY(y),uiCanvasX(x+w)-uiCanvasX(x),uiCanvasY(y+h)-uiCanvasY(y),uiCanvasX(r),c);}
static inline void uiCanvasOutline(int x,int y,int w,int h,int r,unsigned c){tft.drawRoundRect(uiCanvasX(x),uiCanvasY(y),uiCanvasX(x+w)-uiCanvasX(x),uiCanvasY(y+h)-uiCanvasY(y),uiCanvasX(r),c);}
static inline void uiCanvasRect(int x,int y,int w,int h,unsigned c){tft.drawRect(uiCanvasX(x),uiCanvasY(y),uiCanvasX(x+w)-uiCanvasX(x),uiCanvasY(y+h)-uiCanvasY(y),c);}
static inline void uiCanvasLine(int x,int y,int xx,int yy,unsigned c){tft.drawLine(uiCanvasX(x),uiCanvasY(y),uiCanvasX(xx),uiCanvasY(yy),c);}
static int uiCanvasTextX=0,uiCanvasFont=1;
static inline void uiCanvasCursor(int x,int y){uiCanvasTextX=uiCanvasX(x);tft.setCursor(uiCanvasTextX,uiCanvasY(y));}
static inline void uiCanvasTextSize(unsigned n){uiCanvasFont=uiCanvasCompact()?std::min(n,2u):n;tft.setTextSize(uiCanvasFont);}
static inline void uiCanvasPrint(const char*s){
 if(!uiCanvasCompact()){tft.print(s);return;}
 int capacity=std::max(0,(tft.width()-uiCanvasTextX)/(6*uiCanvasFont));
 size_t length=strlen(s),n=std::min<size_t>(length,capacity);char line[81];
 n=std::min(n,sizeof(line)-1);memcpy(line,s,n);line[n]=0;
 if(length>n&&n>=3)memcpy(line+n-3,"...",3);
 tft.print(line);uiCanvasTextX+=n*6*uiCanvasFont;
}
template<class...T>static inline void uiCanvasPrintf(const char*f,T...args){char line[256];snprintf(line,sizeof(line),f,args...);uiCanvasPrint(line);}
