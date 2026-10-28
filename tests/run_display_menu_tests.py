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
struct Button{bool edge=false,pressedEdge=false;bool pressed(){return edge||pressedEdge;}}bUp,bDown,bLeft,bRight,bOk,bFire;
struct {int x=0,y=0;}usbPointer;
bool saw9341=false,saw9488=false;
int hitY=-1,hitX=120,restarts,saves,entered,stops,presents;bool saveOk=true,viewerRole=false;unsigned viewerSwitches=0;
bool usbInputViewerMode(){return viewerRole;}
bool usbInputSwitchViewer(bool on){if(!saveOk)return false;viewerRole=on;viewerSwitches++;return true;}
static freslest_display_t active;
static unsigned fit=90,fitSaves;
static unsigned rawTests,invalidations;
bool freslest_video_color_test(){++rawTests;return true;}
void gfxInvalidate(){++invalidations;}
unsigned freslest_video_fit(){return fit;}
bool freslest_video_save_fit(unsigned value){assert(value>=80&&value<=100&&value%5==0);++fitSaves;if(!saveOk)return false;fit=value;return true;}
bool usbInputHit(int x,int y,int w,int h){if(hitY>=y&&hitY<y+h&&hitX>=x&&hitX<x+w){usbPointer.y=hitY;usbPointer.x=hitX;hitY=-1;return true;}return false;}
freslest_display_t freslest_video_output(){return active;}
bool freslest_video_save_output(freslest_display_t v){assert(v!=active&&v!=1);saves++;return saveOk;}
struct {void restart(){restarts++;}}ESP;
struct Theme{int bg=0,text=1,dim=2,accent=3,selbg=4,panel=5;};Theme theme;Theme& TH(){return theme;}
struct Screen{int w=480;int width(){return w;}void fillScreen(int){}
 void bounds(int x,int y,int ww,int hh){assert(x>=0&&x+ww<=w&&y>=(w==320?36:44)&&y+hh<=(w==320?216:296));}
 void fillRect(int x,int y,int w,int h,int){bounds(x,y,w,h);}void fillRoundRect(int x,int y,int w,int h,int,int){bounds(x,y,w,h);}void drawRoundRect(int x,int y,int w,int h,int,int){bounds(x,y,w,h);}
 void setTextSize(int){}void setTextColor(int,int){}void setCursor(int x,int y){bounds(x,y,1,8);}void print(const char*s){if(strstr(s,"ILI9341"))saw9341=true;if(strstr(s,"ILI9488"))saw9488=true;assert(!strstr(s,"Composite")&&!strstr(s,"AV color")&&!strstr(s,"TV alignment"));}
}tft;
void drawHeader(const char*){}void clearBody(){}void drawFooter(const char*,const char*){}void delay(int){}
void audioStop(){stops++;}void gfxPresent(){presents++;}
#define APP_SETTINGS 2
void enterApp(int a){assert(a==APP_SETTINGS);entered++;}
'''
test=r'''
void edges(){bUp=bDown=bLeft=bRight=bOk=bFire={};hitY=-1;}
void choose(int row){int y=58+row*31+10;hitY=tft.width()==320?36+(y-44)*5/7:y;setDisplayLoop(0);edges();assert(displayChoice==row);}
void ok(){bOk.edge=true;setDisplayLoop(0);edges();}
int main(){
 const int loaded=tft.width()==320?1:0;
 active=FRESLEST_DISPLAY_LCD;setDisplayEnter();assert(saw9341&&saw9488);
 assert(displayChoice==loaded&&!displayConfirm);
 bFire.edge=true;setDisplayLoop(0);edges();assert(!rawTests&&!invalidations);
 choose(1-loaded);ok();assert(!restarts&&!saves&&!viewerSwitches); // different compiled panel is never falsely applied
 choose(2);ok();assert(displayConfirm&&!saves&&!restarts);
 saveOk=false;ok();assert(displaySaveError&&saves==1&&!restarts&&!stops);
 saveOk=true;ok();assert(restarts==1&&saves==2&&stops==1&&presents==1);
 active=FRESLEST_DISPLAY_VGA;setDisplayEnter();assert(displayChoice==2);ok();assert(entered==1&&saves==2);
 bFire.edge=true;setDisplayLoop(0);edges();assert(rawTests==1&&invalidations==1&&saves==2&&!displayConfirm);
 choose(loaded);ok();assert(displayConfirm);ok();assert(restarts==2&&saves==3);
 setDisplayEnter();choose(3);ok();assert(displayConfirm&&!viewerSwitches);
 saveOk=false;ok();assert(displaySaveError&&!viewerSwitches);
 saveOk=true;ok();assert(viewerRole&&viewerSwitches==1);
 setDisplayEnter();choose(4);
 bLeft.edge=true;setDisplayLoop(0);edges();assert(fit==85&&displayChoice==4&&fitSaves==1&&!displayConfirm);
 saveOk=false;bRight.edge=true;setDisplayLoop(0);edges();assert(fit==85&&displaySaveError);
 saveOk=true;ok();assert(fit==90&&!displayConfirm);
 puts("PASS: both LCD models listed; unavailable profile never falsely applied; LCD/VGA/Viewer switching, native/legacy bounds, TV size, NVS failure and restart");
}
'''
if os.environ.get('FRESLEST_TEST_NATIVE_UI'):pre='#define FRESLEST_LCD_MODEL 9341\n'+pre.replace('int w=480;','int w=320;')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'display.cpp';p.write_text(pre+src[src.index('// Compact settings adapter'):src.index('// ===== Settings menu =====')]+src[src.index('// ===== Display output ====='):]+test);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(r/'OS/freslest/src/freslest'),'-I'+str(r/'shared_components/freslest_video/include'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
