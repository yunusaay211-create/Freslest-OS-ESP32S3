"""Exercise the actual Desktop shell against a display and input test double."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / "OS/freslest/src/freslest/desktop.cpp").read_text()
apps = (root / "OS/freslest/src/freslest/apps.cpp").read_text()
header = (root / "OS/freslest/src/freslest/apps.h").read_text()
app_ids = header[header.index("enum AppId"):header.index("};") + 2]
chrome = apps[apps.index("static char headerTitle"):apps.index("// ======================= BOOT")]
platform = r'''
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "usb_input.h"
#include "freslest_video.h"
#include "vga_pixels.h"
#include "ui_layout.h"
#include "desktop.h"
struct Button { bool pressedEdge=false; bool pressed(){return pressedEdge;} } bUp,bDown,bLeft,bRight,bOk,bBack;
''' + app_ids + r'''
UsbPointer usbPointer;
AppId app=APP_MENU;
unsigned loops=0;
bool tab=false;
void enterApp(AppId id){app=id;}
void backToMenu(){app=APP_MENU;}
void appLoop(uint32_t){
 ++loops;
 if(usbPointer.click)assert(usbPointer.x>=0&&usbPointer.x<480&&usbPointer.y>=0&&usbPointer.y<320);
 if(app==APP_FILES&&bBack.pressed())enterApp(APP_APPS);
}
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
bool installNoticeUpdate(uint32_t,bool,bool){return false;}
bool nesPlayerFullscreen(){return false;}
unsigned headerStatuses=0;
void systemStatusDraw(bool){++headerStatuses;}
void drawBatteryTopBar(bool){++headerStatuses;}
uint32_t fpsValue=30;
bool footerDirty=true;
struct Theme {int bg=0,panel=1,text=2,accent=3,dim=4;};
Theme TH(){return {};}
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
  std::vector<std::string> words;
  void setPsram(bool){}
  void setColorDepth(int){}
  void setTextWrap(bool){}
  void setSwapBytes(bool){}
  void *createSprite(int w,int h){assert(w==640&&h==480);sized=true;return pixels;}
  void *getBuffer(){return sized?pixels:nullptr;}
  int width(){return sized?640:480;}
  int height(){return sized?480:320;}
  void setTextColor(int,int=0){}
  void setTextSize(int){}
  void setCursor(int,int){}
  void print(const char* s){words.emplace_back(s);}
  void print(unsigned n){words.emplace_back(std::to_string(n));}
  void printf(const char* format,...){
    char text[128];
    va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    print(text);
  }
  void fillScreen(int){words.clear();}
  void fillCircle(int,int,int,int){}
  void fillTriangle(int,int,int,int,int,int,int){}
  void fillRect(int,int,int,int,int){}
  void fillRoundRect(int,int,int,int,int,int){}
  void pushImage(int x,int y,int w,int h,const uint16_t *p){assert(x>=0&&y>=0&&w==480&&h==320&&p);++appImages;}
};
uint16_t LGFX_Sprite::pixels[640*480];
}
lgfx::LGFX_Sprite tft;
bool desktopHasWindow();
bool desktopVisible();
'''
test = r'''
bool printed(const lgfx::LGFX_Sprite& display,const char* text){
 return std::find(display.words.begin(),display.words.end(),text)!=display.words.end();
}
void click(int x,int y,uint32_t now,bool hold=false){
 usbPointer={x,y,true,hold,true,false,0};desktopUpdate(now);usbPointer.click=false;
}
int main(){
 desktopBegin();assert(!desktopVisible());
 drawHeader("Files");drawFooter("Open","Back");
 assert(printed(tft,"FRESLEST")&&printed(tft,"FPS:")&&headerStatuses==2);
 output=FRESLEST_DISPLAY_VGA;desktopBegin();assert(desktopVisible());
 assert(!desktopHasWindow()&&desktopPixels());
 click(24,32,300);assert(app==APP_MENU);
 click(24,32,500);assert(app==APP_FILES&&desktopHasWindow());
 static uint16_t pixels[480*320];
 assert(desktopCompose(pixels,false)&&canvas.appImages==1&&!desktopCompose(pixels,false));
 assert(printed(canvas,"Start")&&printed(canvas,"Files")&&printed(canvas,"---- -- SND ---  --:--"));
 tft.words.clear();drawHeader("My Computer");drawFooter("Open","Back");
 assert(printed(tft,"My Computer")&&printed(tft,"Back")&&!printed(tft,"FRESLEST")&&!printed(tft,"FPS:")&&headerStatuses==2);
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
 enterApp(APP_FILES);bBack.pressedEdge=true;desktopUpdate(900);bBack.pressedEdge=false;
 assert(app==APP_MENU&&!desktopHasWindow());
 click(20,TaskY+6,1000);
 click(30,MenuTop+9+13*22,1010);
 assert(app==APP_NETWORK_UPDATE&&!desktopVisible()&&!desktopHasWindow());
 assert(desktopCloseModal()&&app==APP_MENU&&desktopVisible()&&!count);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / "desktop.cpp"
    path.write_text(platform + chrome + src[src.index("namespace {"):] + test)
    exe = path.with_suffix("")
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-pie", "-no-pie", "-I", root / "OS/freslest",
                    "-I", root / "shared_components/freslest_video/include",
                    "-I", root / "OS/freslest/src/freslest", path, "-o", exe], check=True)
    subprocess.run([exe], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
print("PASS: English VGA Desktop, app-only window chrome, pointer mapping, focus, close and fullscreen update handoff")
