"""Exercise the actual Desktop shell against a display and input test double."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "OS/freslest/src/freslest/desktop.cpp").read_text()
platform = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "usb_input.h"
#include "freslest_video.h"
#include "vga_pixels.h"
struct Button { bool pressedEdge=false; bool pressed(){return pressedEdge;} } bUp,bDown,bLeft,bRight,bOk,bBack;
enum AppId {APP_MENU,APP_FILES,APP_SETTINGS,APP_FRESTORE,APP_TERMINAL,APP_GAME_MENU,APP_FRESTORE_APPS,
  APP_APPS,APP_FRESLEST_UPDATE,APP_SYSINFO,APP_TEST,APP_CAMERA,APP_USB_TRANSFER,APP_DINO,APP_TETRIS,
  APP_SNAKE,APP_MINES,APP_2048,APP_DOOM,APP_NES_PLAYER,APP_BROWSER};
UsbPointer usbPointer;
AppId app=APP_MENU;
unsigned loops=0;
bool tab=false;
void enterApp(AppId id){app=id;}
void backToMenu(){app=APP_MENU;}
void appLoop(uint32_t){++loops;if(usbPointer.click)assert(usbPointer.x>=0&&usbPointer.x<480&&usbPointer.y>=0&&usbPointer.y<320);}
bool textEntryActive(){return false;}
bool usbInputTasks(){return tab;}
bool usbInputAttached(){return false;}
void usbInputTextMode(bool){}
int usbInputCharacter(){return -1;}
bool rtcGetLocal(struct tm&){return false;}
struct WiFiType {int status(){return 0;}} WiFi;
constexpr int WL_CONNECTED=3;
struct BleInfo {int connected;};
BleInfo freslestBleInfo(){return {0};}
struct UsbStorage {bool present(){return false;}};
UsbStorage usbStorageInfo(){return {};}
bool soundEnabled=true;
const char *installNoticeText(){return nullptr;}
struct SerialType{void println(const char*){}} Serial;
freslest_display_t output=FRESLEST_DISPLAY_LCD;
freslest_display_t freslest_video_output(){return output;}
void freslest_video_diagnostics(){}
const char *freslest_hid_status(){return "test";}
struct Version{const char* version="1.5";};
const Version* esp_app_get_description(){static Version v;return &v;}
int constrain(int v,int lo,int hi){return std::clamp(v,lo,hi);}
namespace lgfx {
struct LGFX_Sprite {
  static uint16_t pixels[640*480];
  bool sized=false;
  unsigned appImages=0;
  void setPsram(bool){}
  void setColorDepth(int){}
  void setTextWrap(bool){}
  void setSwapBytes(bool){}
  void *createSprite(int w,int h){assert(w==640&&h==480);sized=true;return pixels;}
  void *getBuffer(){return sized?pixels:nullptr;}
  void setTextColor(int){}
  void setTextSize(int){}
  void setCursor(int,int){}
  void print(const char*){}
  void printf(const char*,...){}
  void fillScreen(int){}
  void fillCircle(int,int,int,int){}
  void fillTriangle(int,int,int,int,int,int,int){}
  void fillRect(int,int,int,int,int){}
  void fillRoundRect(int,int,int,int,int,int){}
  void pushImage(int x,int y,int w,int h,const uint16_t *p){assert(x>=0&&y>=0&&w==480&&h==320&&p);++appImages;}
};
uint16_t LGFX_Sprite::pixels[640*480];
}
lgfx::LGFX_Sprite tft;
void drawHeader(const char*){}
void drawFooter(const char*,const char*){}
'''
test = r'''
void click(int x,int y,uint32_t now,bool hold=false){
 usbPointer={x,y,true,hold,true,false,0};desktopUpdate(now);usbPointer.click=false;
}
int main(){
 desktopBegin();assert(!desktopVisible());
 output=FRESLEST_DISPLAY_VGA;desktopBegin();assert(desktopVisible());
 assert(!desktopHasWindow()&&desktopPixels());
 click(24,32,300);assert(app==APP_MENU);
 click(24,32,500);assert(app==APP_FILES&&desktopHasWindow());
 static uint16_t pixels[480*320];
 assert(desktopCompose(pixels,false)&&canvas.appImages==1&&!desktopCompose(pixels,false));
 auto original=windows[0];
 click(original.x+20,original.y+8,600,true);
 assert(dragging);
 usbPointer.x=630;usbPointer.y=350;desktopUpdate(650);
 assert(windows[0].x==640-WinWidth&&windows[0].y==TaskY-WinHeight);
 usbPointer.left=false;desktopUpdate(660);assert(!dragging);
 auto moved=windows[0];
 click(moved.x+25,moved.y+Title+25,680);
 assert(loops==1&&usbPointer.x==moved.x+25);
 click(20,TaskY+6,700);assert(menu);
 click(30,MenuTop+4+22+5,710);assert(app==APP_SETTINGS&&count==2);
 tab=true;desktopUpdate(720);tab=false;assert(app==APP_FILES);
 auto front=windows[count-1];
 click(front.x+WinWidth-8,front.y+7,800);assert(app==APP_SETTINGS&&count==1);
 enterApp(APP_NES_PLAYER);assert(!desktopVisible());
 enterApp(APP_GAME_MENU);assert(desktopVisible());
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / "desktop.cpp"
    path.write_text(platform + src[src.index("namespace {"):] + test)
    exe = path.with_suffix("")
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-pie", "-no-pie", "-I", root / "OS/freslest",
                    "-I", root / "shared_components/freslest_video/include", path, "-o", exe], check=True)
    subprocess.run([exe], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
print("PASS: VGA-only Desktop, double-click, drag bounds, focus, close, pointer mapping and fullscreen handoff")
