"""Use the bundled LovyanGFX converter to catch RGB565->RGB888 promotion."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
code=r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include "lgfx/v1/misc/colortype.hpp"
#define FRESLEST_LCD_MODEL 9341
struct Screen{
 lgfx::color_conv_t converter;uint32_t raw=0;long width(){return 320;}long height(){return 240;}
 template<class C>void fillRect(int,int,int,int,C c){raw=converter.convert(c);}
 template<class C>void fillRoundRect(int,int,int,int,int,C c){raw=converter.convert(c);}
 template<class C>void drawRoundRect(int,int,int,int,int,C c){raw=converter.convert(c);}
 template<class C>void drawRect(int,int,int,int,C c){raw=converter.convert(c);}
 template<class C>void drawLine(int,int,int,int,C c){raw=converter.convert(c);}
 void setCursor(int,int){}void setTextSize(int){}void print(const char*){}
}tft;
struct {int x=0,y=0;}usbPointer;bool usbInputHit(int,int,int,int){return false;}
#include "ui_canvas.h"
#include "settings_canvas.h"
int main(){
 for(uint16_t c:{uint16_t(0),uint16_t(0xffff),uint16_t(0x07e0),uint16_t(0xf800),uint16_t(0x10a2),uint16_t(0xa514),uint16_t(0x2104),uint16_t(0x07ff)}){
  auto expected=tft.converter.convert(c);
  uiCanvasFillRect(20,80,100,30,c);assert(tft.raw==expected);
  uiCanvasRoundRect(20,80,100,30,5,c);assert(tft.raw==expected);
  uiCanvasOutline(20,80,100,30,5,c);assert(tft.raw==expected);
  uiCanvasRect(20,80,100,30,c);assert(tft.raw==expected);
  uiCanvasLine(20,80,40,100,c);assert(tft.raw==expected);
  settingFillRect(20,80,100,30,c);assert(tft.raw==expected);
  settingFillRoundRect(20,80,100,30,5,c);assert(tft.raw==expected);
  settingDrawRoundRect(20,80,100,30,5,c);assert(tft.raw==expected);
 }
 uint32_t rgb=0x55cc33;uiCanvasFillRect(20,80,100,30,rgb);assert(tft.raw==tft.converter.convert(rgb));
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'colors.cpp';p.write_text(code);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(r/'OS/components/LovyanGFX/src'),'-I'+str(r/'OS/freslest/src/freslest'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: real LovyanGFX converter preserves RGB565 and explicit RGB888 through app/settings adapters')
