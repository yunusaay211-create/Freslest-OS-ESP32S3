#pragma once
// Compact settings adapter: geometry maps into the body; glyphs stay native.
#if FRESLEST_LCD_MODEL == 9341
static inline bool settingCompact(){return tft.width()==320;}
static inline int settingX(int x){return settingCompact()?x*2/3:x;}
static inline int settingY(int y){return settingCompact()?36+(y-44)*5/7:y;}
static inline int settingW(int w){return settingCompact()?w*2/3:w;}
static inline int settingH(int h){return settingCompact()?h*5/7:h;}
static inline void settingFillRect(int x,int y,int w,int h,uint32_t c){tft.fillRect(settingX(x),settingY(y),settingW(w),settingH(h),c);}
static inline void settingFillRoundRect(int x,int y,int w,int h,int r,uint32_t c){tft.fillRoundRect(settingX(x),settingY(y),settingW(w),settingH(h),settingW(r),c);}
static inline void settingDrawRoundRect(int x,int y,int w,int h,int r,uint32_t c){tft.drawRoundRect(settingX(x),settingY(y),settingW(w),settingH(h),settingW(r),c);}
static inline void settingCursor(int x,int y){tft.setCursor(settingX(x),settingY(y));}
static inline void settingTextSize(int n){tft.setTextSize(settingCompact()?(n>=3?2:1):n);}
static inline bool settingHit(int x,int y,int w,int h){return usbInputHit(settingX(x),settingY(y),settingW(w),settingH(h));}
static inline int settingPointerX(){return settingCompact()?usbPointer.x*3/2:usbPointer.x;}
static inline int settingPointerY(){return settingCompact()?44+(usbPointer.y-36)*7/5:usbPointer.y;}
#else
static inline bool settingCompact(){return false;}
#define settingFillRect tft.fillRect
#define settingFillRoundRect tft.fillRoundRect
#define settingDrawRoundRect tft.drawRoundRect
#define settingCursor tft.setCursor
#define settingTextSize tft.setTextSize
#define settingHit usbInputHit
#define settingPointerX() usbPointer.x
#define settingPointerY() usbPointer.y
#endif

