"""Run the actual Task Switcher handler and Game menu draw functions on host."""
from pathlib import Path
import subprocess,os
base=Path(__file__).resolve().parent;app=base.parent/'freslest/src/freslest';build=base/'host_build';build.mkdir(exist_ok=True)
def function(source,name):
    start=source.index(name);brace=source.index('{',start);end=brace+1;depth=1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
source=(app/'apps.cpp').read_text();menus=(app/'menus_sub.cpp').read_text();header=(app/'apps.h').read_text()
enum=header[header.index('enum AppId'):header.index('};')+2]
items=menus[menus.index('static const char* GAME_ITEMS'):menus.index('static void drawGameRow')]
appitems=menus[menus.index('static const char* APPS_ITEMS'):menus.index('// ===== Game menu =====')]
prelude=r'''
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "task_hold.h"
struct Button {bool state=false,edge=false,pressedEdge=false;bool down()const{return state;}bool pressed()const{return edge||pressedEdge;}};
Button bOk,bUp,bDown,bLeft,bRight,bBack;
struct Pointer{int x=0,y=0;bool click=false;}usbPointer;
bool usbInputTasks(){return false;}
bool usbInputHit(int x,int y,int w,int h){return usbPointer.click&&usbPointer.x>=x&&usbPointer.x<x+w&&usbPointer.y>=y&&usbPointer.y<y+h;}

bool taskView=false,taskInputConsumed=false,nesFullscreen=false;
TaskHold taskHold;constexpr unsigned OK_HOLD_MS=650;
unsigned switcherDraws=0,menuDraws=0,entered=0;
struct Theme {unsigned bg=0,text=1,dim=2,accent=3,selbg=4;};
Theme& TH(){static Theme t;return t;}
struct Display {
 int h=320;int height(){return h;}
 int cursorX=0,cursorY=0,size=1;std::vector<std::string> words;
 void fillScreen(unsigned){}void fillRect(int,int y,int,int h,unsigned){assert(y>=44&&y+h<=this->h-24);}
 void fillRoundRect(int,int y,int,int h,int,unsigned){assert(y>=44&&y+h<=this->h-24);}
 void drawRoundRect(int,int y,int,int h,int,unsigned){assert(y>=44&&y+h<=this->h-24);}
 void setTextColor(unsigned,unsigned){}void setTextSize(int n){size=n;}
 void setCursor(int x,int y){cursorX=x;cursorY=y;assert(y>=44&&y+8*size<=h-24);}
 void print(const char* text){words.emplace_back(text);cursorX+=strlen(text)*6*size;assert(cursorX<=480);}
}tft;
void drawHeader(const char*){}void drawFooter(const char*,const char*){}void clearBody(){}void drawAppsMenuFull();
void audioClick(){}void nesPlayerExit(){}void noteExit(){}
bool editorActive=false;unsigned browserPauses=0;bool textEntryActive(){return editorActive;}void browserExit(){browserPauses++;}
void drawMenuFull(){++menuDraws;}void drawTaskSwitcherFull(){++switcherDraws;}
'''
globals=r'''
AppId app=APP_MENU,tasks[]={APP_CALC,APP_NOTE,APP_CLOCK};int taskCount=3,taskSel=0;
void enterApp(AppId next){app=next;++entered;}
'''
test=r'''
static void edges(){bOk.edge=bUp.edge=bDown.edge=bLeft.edge=bRight.edge=bBack.edge=false;bOk.pressedEdge=false;}
int main(){
 bOk={true,true};taskSwitcherHandle(100);edges();taskSwitcherHandle(749);assert(!taskView);
 taskSwitcherHandle(750);assert(taskView&&taskInputConsumed&&switcherDraws==1);
 taskSwitcherHandle(3000);assert(taskView&&!entered);
 bOk.state=false;taskSwitcherHandle(3001);
 bOk={true,true};taskSwitcherHandle(3002);assert(!taskView&&taskInputConsumed&&app==APP_CALC&&entered==1);
 edges();taskSwitcherHandle(5000);assert(!taskView&&switcherDraws==1);
 bOk.state=false;taskSwitcherHandle(5001);bOk={true,true};taskSwitcherHandle(5100);edges();
 taskSwitcherHandle(5750);assert(taskView&&switcherDraws==2);
 bOk.state=false;taskSwitcherHandle(5751);
 // Simultaneous navigation+selection used to redraw the switcher over the app.
 bRight.edge=true;bOk={true,true};taskSwitcherHandle(5752);
 assert(!taskView&&taskInputConsumed&&app==APP_NOTE&&switcherDraws==2);edges();
 taskSwitcherHandle(9000);assert(!taskView);
 bOk.state=false;taskSwitcherHandle(9001);bOk.state=true;taskSwitcherHandle(9100);taskSwitcherHandle(9750);
 assert(taskView);bOk.state=false;bBack.edge=true;taskSwitcherHandle(9751);
 assert(!taskView&&taskInputConsumed&&app==APP_NOTE);edges();
 taskView=true;taskHold.blockUntilRelease();usbPointer={185,130,true};taskSwitcherHandle(9800);assert(!taskView&&taskInputConsumed&&app==APP_NOTE);usbPointer.click=false;edges();
 drawGameMenuFull();assert(GAME_COUNT==7&&!strcmp(GAME_ITEMS[6],"NES Library"));assert(std::find(tft.words.begin(),tft.words.end(),"Fullscreen")==tft.words.end());
 appsIndex=7;drawAppsMenuFull();assert(appsTop==1&&appsVisible()==7);assert(__appsMenuRowAt(54)==1&&__appsMenuRowAt(258)==7&&__appsMenuRowAt(300)==-1);
 appsIndex=0;drawAppsMenuSelectionOnly(7,0);assert(appsTop==0);assert(appsMenuToApp(6)==APP_BROWSER&&appsMenuToApp(7)==APP_SETTINGS);
 // The TV canvas fits all eight app rows without hiding the footer.
 tft.h=360;appsIndex=7;drawAppsMenuFull();assert(appsTop==0&&appsVisible()==8&&__appsMenuRowAt(292)==7);tft.h=320;
 taskView=false;app=APP_BROWSER;taskHold=TaskHold{};bOk={true,true};taskSwitcherHandle(10000);edges();taskSwitcherHandle(10650);assert(taskView&&browserPauses==1);taskView=false;
 editorActive=true;taskHold=TaskHold{};taskSwitcherHandle(11000);taskSwitcherHandle(12000);assert(!taskView);editorActive=false;
 puts("PASS: Browser pauses for Task Switcher; text editing blocks long-OK task switching");
 puts("PASS: Apps Browser entry, LCD scrolling/mouse row mapping and eight-row 4:3 TV bounds");
 puts("PASS: actual Task Switcher hold/release/selection/mouse/BACK and seven Game rows; Fullscreen removed from Game");
}
'''
code=prelude+enum+'\n'+globals+function(source,'static void removeTaskAt(')+'\n'+function(source,'void taskSwitcherHandle(')+'\n'+appitems+'\n'+function(menus,'int __appsMenuRowAt(')+'\n'+items+'\n'+function(menus,'static void drawGameRow(')+'\n'+function(menus,'void drawGameMenuFull(')+'\n'+function(menus,'void drawGameMenuSelectionOnly(')+'\n'+test
p=build/'ui_actual.cpp';p.write_text(code)
binary=build/'test_ui_actual'
subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(app),str(p),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
