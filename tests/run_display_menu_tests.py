from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];src=(r/'OS/freslest/src/freslest/settings_pages.cpp').read_text()
code=src[src.index('// ===== Display output ====='):]
pre=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <freslest_video.h>
struct Button{bool edge=false,pressedEdge=false;bool pressed(){return edge||pressedEdge;}}bUp,bDown,bLeft,bRight,bOk,bFire;
struct {int x=0,y=0;}usbPointer;
static int hitY=-1,hitX=240,restarts,saves,entered,stops,presents;static bool saveOk=true,viewerRole=false;static unsigned viewerSwitches=0;
bool usbInputViewerMode(){return viewerRole;}
bool usbInputSwitchViewer(bool on){if(!saveOk)return false;viewerRole=on;viewerSwitches++;return true;}
static freslest_display_t active;
bool usbInputHit(int x,int y,int w,int h){if(hitY>=y&&hitY<y+h&&hitX>=x&&hitX<x+w){usbPointer.y=hitY;usbPointer.x=hitX;hitY=-1;return true;}return false;}
freslest_display_t freslest_video_output(){return active;}
bool freslest_video_save_output(freslest_display_t v){assert(v!=active);saves++;return saveOk;}
static freslest_tv_config_t config={0,0,100},committed={0,0,100};static int tvSaves;
freslest_tv_config_t freslest_video_tv_config(){return config;}
void freslest_video_tv_preview(freslest_tv_config_t c){config=c;}
bool freslest_video_tv_save(freslest_tv_config_t c){tvSaves++;if(!saveOk)return false;committed=config=c;return true;}
struct {void restart(){restarts++;}}ESP;
struct Theme{int bg=0,text=1,dim=2,accent=3,selbg=4,panel=5;};Theme theme;Theme& TH(){return theme;}
struct {void fillScreen(int){}void fillRect(int,int y,int,int h,int){assert(y>=44&&y+h<=296);}void fillRoundRect(int,int y,int,int h,int,int){assert(y>=44&&y+h<=296);}void setTextSize(int){}void setTextColor(int,int){}void setCursor(int,int y){assert(y>=44&&y<296);}void print(const char*){}void printf(const char*,...){}}tft;
void drawHeader(const char*){}void clearBody(){}void drawFooter(const char*,const char*){}void delay(int){}
void audioStop(){stops++;}void gfxPresent(){presents++;}
#define TFT_RED 0xf800
#define APP_SETTINGS 2
#define APP_SET_DISPLAY 31
#define APP_SET_TV 32
#define APP_SET_TV_COLOR 33
static int app=APP_SET_DISPLAY;
static freslest_tv_color_t color={0,100,0},savedColor={0,100,0};static int colorSaves;
freslest_tv_color_t freslest_video_color_config(){return color;}
void freslest_video_color_preview(freslest_tv_color_t c){color=c;}
bool freslest_video_color_save(freslest_tv_color_t c){colorSaves++;if(!saveOk)return false;return savedColor=color=c,true;}
void setTvColorExit();
void setTvExit();
void enterApp(int a){assert(a==APP_SETTINGS||a==APP_SET_TV||a==APP_SET_DISPLAY||a==APP_SET_TV_COLOR);if(app==APP_SET_TV_COLOR&&a!=APP_SET_TV_COLOR)setTvColorExit();if(app==APP_SET_TV&&a!=APP_SET_TV)setTvExit();app=a;entered++;}
'''
test=r'''
void edges(){bUp=bDown=bLeft=bRight=bOk=bFire={};hitY=-1;hitX=240;}
int main(){
 active=FRESLEST_DISPLAY_LCD;setDisplayEnter();assert(displayChoice==0&&!displayConfirm);
 bDown.edge=true;setDisplayLoop(0);edges();assert(displayChoice==1&&!saves&&!restarts);
 bOk.edge=true;setDisplayLoop(1);edges();assert(displayConfirm&&!saves&&!restarts);
 setDisplayEnter();assert(!displayConfirm&&displayChoice==0&&!saves);
 hitY=100;setDisplayLoop(2);edges();assert(displayChoice==1&&!displayConfirm);
 hitY=250;setDisplayLoop(3);edges();assert(displayConfirm&&!saves);
 saveOk=false;hitY=250;setDisplayLoop(4);edges();assert(saves==1&&displaySaveError&&!restarts&&!stops);
 saveOk=true;bOk.edge=true;setDisplayLoop(5);edges();assert(saves==2&&restarts==1&&stops==1&&presents==1);
 active=FRESLEST_DISPLAY_COMPOSITE;setDisplayEnter();assert(displayChoice==1&&!displayConfirm&&!displaySaveError);
 bOk.edge=true;setDisplayLoop(6);edges();assert(entered==1&&saves==2);
 bUp.edge=true;setDisplayLoop(7);edges();assert(displayChoice==0&&!displayConfirm);
 bOk.edge=true;setDisplayLoop(8);edges();assert(displayConfirm&&saves==2);
 bOk.edge=true;setDisplayLoop(9);edges();assert(restarts==2&&saves==3);
 setDisplayEnter();bDown.edge=true;setDisplayLoop(10);edges();assert(displayChoice==2);
 bOk.edge=true;setDisplayLoop(11);edges();assert(app==APP_SET_TV&&entered==2);
 setTvEnter();tvRow=2;bLeft.edge=true;setTvLoop(12);edges();assert(config.scale==98&&committed.scale==100&&!tvSaves);
 tvRow=0;bRight.edge=true;setTvLoop(13);edges();assert(config.x==1);
 enterApp(APP_SET_DISPLAY);assert(config.x==0&&config.scale==100&&!tvSaves);
 app=APP_SET_TV;setTvEnter();tvRow=2;bLeft.edge=true;setTvLoop(14);edges();
 saveOk=false;bOk.edge=true;setTvLoop(15);edges();assert(tvSaveError&&app==APP_SET_TV&&committed.scale==100&&tvSaves==1);
 saveOk=true;hitY=250;setTvLoop(16);edges();assert(app==APP_SET_DISPLAY&&committed.scale==98&&config.scale==98&&tvSaves==2);
 setDisplayEnter();hitY=125;setDisplayLoop(17);edges();assert(app==APP_SET_TV);
 setTvEnter();hitX=410;hitY=105;setTvLoop(18);edges();assert(tvRow==1&&config.y==1);
 hitY=180;setTvLoop(19);edges();assert(config.scale==100&&config.y==0&&committed.scale==98);
 enterApp(APP_SET_DISPLAY);assert(config.scale==98&&config.y==0);
 setDisplayEnter();hitY=160;setDisplayLoop(20);edges();assert(app==APP_SET_TV_COLOR);
 setTvColorEnter();bRight.edge=true;setTvColorLoop(21);edges();assert(color.hue==5&&savedColor.hue==0);
 enterApp(APP_SET_DISPLAY);assert(color.hue==0&&!colorSaves);
 app=APP_SET_TV_COLOR;setTvColorEnter();colorRow=1;bLeft.edge=true;setTvColorLoop(22);edges();assert(color.saturation==95);
 saveOk=false;bOk.edge=true;setTvColorLoop(23);edges();assert(colorSaveError&&colorSaves==1&&savedColor.saturation==100);
 saveOk=true;hitY=270;setTvColorLoop(24);edges();assert(app==APP_SET_DISPLAY&&savedColor.saturation==95&&colorSaves==2);
 app=APP_SET_TV_COLOR;setTvColorEnter();hitY=60;hitX=410;setTvColorLoop(25);edges();assert(color.hue==5);
 colorRow=0;for(int i=0;i<50;i++){bRight.edge=true;setTvColorLoop(26);edges();}assert(color.hue==180);
 colorRow=1;for(int i=0;i<50;i++){bLeft.edge=true;setTvColorLoop(27);edges();}assert(color.saturation==0);
 bFire.edge=true;setTvColorLoop(28);edges();assert(color.hue==0&&color.saturation==100&&savedColor.saturation==95);
 enterApp(APP_SET_DISPLAY);assert(color.saturation==95);
 // The old rendering remains selectable; Clean is the reset default.
 app=APP_SET_TV_COLOR;setTvColorEnter();colorRow=2;bRight.edge=true;setTvColorLoop(29);edges();assert(color.detail==FRESLEST_TV_SHARP);
 bOk.edge=true;setTvColorLoop(30);edges();assert(savedColor.detail==FRESLEST_TV_SHARP&&app==APP_SET_DISPLAY);
 app=APP_SET_TV_COLOR;setTvColorEnter();hitX=320;hitY=120;setTvColorLoop(31);edges();assert(colorRow==2&&color.detail==FRESLEST_TV_CLEAN);
 enterApp(APP_SET_DISPLAY);assert(color.detail==FRESLEST_TV_SHARP);
 app=APP_SET_TV_COLOR;setTvColorEnter();colorRow=3;bOk.edge=true;setTvColorLoop(32);edges();assert(color.hue==0&&color.saturation==100&&color.detail==FRESLEST_TV_CLEAN&&app==APP_SET_TV_COLOR);
 bUp.edge=true;setTvColorLoop(33);edges();assert(colorRow==2);bDown.edge=true;setTvColorLoop(34);edges();assert(colorRow==3);bDown.edge=true;setTvColorLoop(35);edges();assert(colorRow==0);
 enterApp(APP_SET_DISPLAY);assert(color.detail==FRESLEST_TV_SHARP);
 setDisplayEnter();hitY=192;setDisplayLoop(36);edges();assert(displayChoice==4&&!displayConfirm);
 bOk.edge=true;setDisplayLoop(37);edges();assert(displayConfirm&&!viewerSwitches);
 saveOk=false;bOk.edge=true;setDisplayLoop(38);edges();assert(displaySaveError&&!viewerSwitches);
 saveOk=true;bOk.edge=true;setDisplayLoop(39);edges();assert(viewerRole&&viewerSwitches==1);
 puts("PASS: confirmed native PC Viewer role and failed role save preserve settings");
 puts("PASS: actual AV colour page, colour bars/grey ramp, hue/saturation bounds, Clean/Sharp and reset rows, mouse, preview, save failure/retry, reset and BACK cancellation");
 puts("PASS: actual Display selection/confirmation/cancel/save failure/restart; TV alignment buttons and mouse, live preview, save failure/retry, committed state and cancel restoration without restart");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'display.cpp';p.write_text(pre+code+test);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'shared_components/freslest_video/include'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
