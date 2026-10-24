from pathlib import Path
import subprocess,os
base=Path(__file__).resolve().parent;src=(base.parent/'freslest/src/freslest/nes_library.cpp').read_text();build=base/'host_build';build.mkdir(exist_ok=True)
def function(name):
 a=src.index(name);b=src.index('{',a);end=b+1;depth=1
 while depth:depth+=(src[end]=='{')-(src[end]=='}');end+=1
 return src[a:end]
pre=r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
struct String{std::string s;String(const char* p=""):s(p){}String(std::string p):s(p){}size_t length()const{return s.size();}String substring(int a,int b){return String(s.substr(a,b-a));}String operator+(const char* p){return String(s+p);}operator const char*()const{return s.c_str();}};
struct Rom{String name="Test ROM",error="Bad ROM";bool valid=true;unsigned mapper=0,prgKB=16,chrKB=8;};Rom roms[32];
int romCount=0,selected=0,topIndex=0;constexpr int LIST_Y=56,ROW_H=38,VISIBLE_ROWS=4;bool nesFullscreen=false;
struct Theme{unsigned bg=0,panel=1,selbg=2,text=3,dim=4,accent=5;};Theme th;Theme& TH(){return th;}constexpr unsigned TFT_RED=0xf800;
struct Button{bool edge=false;bool pressed(){return edge;}}bFire,bUp,bDown,bLeft,bRight,bOk;
struct Pointer{int x=0,y=0;bool click=false;}usbPointer;
bool usbInputHit(int x,int y,int w,int h){return usbPointer.click&&usbPointer.x>=x&&usbPointer.x<x+w&&usbPointer.y>=y&&usbPointer.y<y+h;}
struct Display{
 std::vector<std::string> words;int size=1,x=0;
 void fillScreen(unsigned){}void setTextSize(int n){size=n;}void setTextColor(unsigned,unsigned){}
 void fillRoundRect(int,int y,int,int h,int,unsigned){assert(y>=44&&y+h<=296);}void drawRoundRect(int,int y,int,int h,int,unsigned){assert(y>=44&&y+h<=296);}
 void setCursor(int xx,int y){x=xx;assert(y>=44&&y+8*size<=296);}void print(const char* s){words.emplace_back(s);x+=strlen(s)*6*size;assert(x<=480);}
 template<class... A>void printf(const char* fmt,A... a){char s[128];snprintf(s,sizeof(s),fmt,a...);print(s);}
}tft;
void drawHeader(const char*){}void clearBody(){}void drawFooter(const char*,const char*){}
int saves=0,entered=0;void settingsSave(){++saves;}void enterApp(int a){entered=a;}constexpr int APP_USB_TRANSFER=1,APP_NES_PLAYER=2;
int constrain(int n,int lo,int hi){return std::max(lo,std::min(n,hi));}
'''
main=r'''
int main(){
 drawLibraryFull();assert(std::find(tft.words.begin(),tft.words.end(),"Fullscreen")!=tft.words.end());
 usbPointer={100,276,true};nesLibraryLoop(0);assert(nesFullscreen&&saves==1&&!entered);usbPointer.click=false;
 bOk.edge=true;nesLibraryLoop(0);bOk.edge=false;assert(!nesFullscreen&&saves==2);
 romCount=8;selected=7;ensureSelectionVisible();drawLibraryFull();assert(topIndex==4);
 usbPointer={60,248,true};nesLibraryLoop(0);assert(entered==APP_USB_TRANSFER);entered=0;
 usbPointer={60,276,true};nesLibraryLoop(0);assert(nesFullscreen&&saves==3&&!entered);
 puts("PASS: actual NES menu has visible final Fullscreen row with zero/many ROMs, click/OK persistence, paged rows and separate USB action");
}
'''
p=build/'nes_menu_actual.cpp';p.write_text(pre+function('void ensureSelectionVisible(')+function('void drawLibraryFull(')+function('void nesLibraryLoop(')+main);binary=build/'test_nes_menu'
subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined',str(p),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
