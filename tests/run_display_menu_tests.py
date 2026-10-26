"""Exercise actual LCD/VGA/Viewer selection with native and legacy geometry."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];src=(r/'OS/freslest/src/freslest/settings_pages.cpp').read_text()
pre=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <freslest_video.h>
struct Button{bool edge=false,pressedEdge=false;bool pressed(){return edge||pressedEdge;}}bUp,bDown,bLeft,bRight,bOk;
struct {int x=0,y=0;}usbPointer;
int hitY=-1,hitX=120,restarts,saves,entered,stops,presents;bool saveOk=true,viewerRole=false;unsigned viewerSwitches=0;
bool usbInputViewerMode(){return viewerRole;}
bool usbInputSwitchViewer(bool on){if(!saveOk)return false;viewerRole=on;viewerSwitches++;return true;}
static freslest_display_t active;
bool usbInputHit(int x,int y,int w,int h){if(hitY>=y&&hitY<y+h&&hitX>=x&&hitX<x+w){usbPointer.y=hitY;usbPointer.x=hitX;hitY=-1;return true;}return false;}
freslest_display_t freslest_video_output(){return active;}
bool freslest_video_save_output(freslest_display_t v){assert(v!=active&&v!=1);saves++;return saveOk;}
struct {void restart(){restarts++;}}ESP;
struct Theme{int bg=0,text=1,dim=2,accent=3,selbg=4,panel=5;};Theme theme;Theme& TH(){return theme;}
struct Screen{int w=480;int width(){return w;}void fillScreen(int){}
 void bounds(int x,int y,int ww,int hh){assert(x>=0&&x+ww<=w&&y>=(w==320?36:44)&&y+hh<=(w==320?216:296));}
 void fillRect(int x,int y,int w,int h,int){bounds(x,y,w,h);}void fillRoundRect(int x,int y,int w,int h,int,int){bounds(x,y,w,h);}void drawRoundRect(int x,int y,int w,int h,int,int){bounds(x,y,w,h);}
 void setTextSize(int){}void setTextColor(int,int){}void setCursor(int x,int y){bounds(x,y,1,8);}void print(const char*s){assert(!strstr(s,"Composite")&&!strstr(s,"AV color")&&!strstr(s,"TV alignment"));}
}tft;
void drawHeader(const char*){}void clearBody(){}void drawFooter(const char*,const char*){}void delay(int){}
void audioStop(){stops++;}void gfxPresent(){presents++;}
#define APP_SETTINGS 2
void enterApp(int a){assert(a==APP_SETTINGS);entered++;}
'''
test=r'''
void edges(){bUp=bDown=bLeft=bRight=bOk={};hitY=-1;}
int main(){
 active=FRESLEST_DISPLAY_LCD;setDisplayEnter();assert(displayChoice==0&&!displayConfirm);
 bDown.edge=true;setDisplayLoop(0);edges();assert(displayChoice==1&&!saves&&!restarts);
 bOk.edge=true;setDisplayLoop(1);edges();assert(displayConfirm&&!saves&&!restarts);
 setDisplayEnter();assert(!displayConfirm&&displayChoice==0);
 hitY=tft.width()==320?80:100;setDisplayLoop(2);edges();assert(displayChoice==1);
 hitY=tft.width()==320?185:250;setDisplayLoop(3);edges();assert(displayConfirm&&!saves);
 saveOk=false;bOk.edge=true;setDisplayLoop(4);edges();assert(displaySaveError&&saves==1&&!restarts&&!stops);
 saveOk=true;bOk.edge=true;setDisplayLoop(5);edges();assert(restarts==1&&saves==2&&stops==1&&presents==1);
 active=FRESLEST_DISPLAY_VGA;setDisplayEnter();assert(displayChoice==1);
 bOk.edge=true;setDisplayLoop(6);edges();assert(entered==1&&saves==2);
 bUp.edge=true;setDisplayLoop(7);edges();assert(displayChoice==0);
 bOk.edge=true;setDisplayLoop(8);edges();assert(displayConfirm);bOk.edge=true;setDisplayLoop(9);edges();assert(restarts==2&&saves==3);
 setDisplayEnter();bDown.edge=true;setDisplayLoop(10);edges();assert(displayChoice==2);
 bOk.edge=true;setDisplayLoop(11);edges();assert(displayConfirm&&!viewerSwitches);
 saveOk=false;bOk.edge=true;setDisplayLoop(12);edges();assert(displaySaveError&&!viewerSwitches);
 saveOk=true;bOk.edge=true;setDisplayLoop(13);edges();assert(viewerRole&&viewerSwitches==1);
 puts("PASS: actual LCD/VGA/USB Viewer selection, native/legacy bounds, NVS failure, two-step confirmation and restart; AV controls removed");
}
'''
if os.environ.get('FRESLEST_TEST_NATIVE_UI'):pre='#define FRESLEST_LCD_MODEL 9341\n'+pre.replace('int w=480;','int w=320;')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'display.cpp';p.write_text(pre+src[src.index('// Compact settings adapter'):src.index('// ===== Settings menu =====')]+src[src.index('// ===== Display output ====='):]+test);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(r/'OS/freslest/src/freslest'),'-I'+str(r/'shared_components/freslest_video/include'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
