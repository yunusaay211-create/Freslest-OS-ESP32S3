"""Render actual Calculator/Note/Clock code and check native bounds and input."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];a=r/'OS/freslest/src/freslest'
def fn(s,name):
 start=s.index(name);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]+'\n'
pre=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <string>
#include <algorithm>
#include <type_traits>
#include "ui_layout.h"
#include <cstdlib>
struct String {std::string s;String(const char*p=""):s(p){}String(std::string p):s(p){}String(char c):s(1,c){}String(int32_t n):s(std::to_string(n)){}
 size_t length()const{return s.size();}const char*c_str()const{return s.c_str();}char operator[](int i)const{return s[i];}
 void setCharAt(int i,char c){s[i]=c;}void remove(size_t at,size_t count=std::string::npos){s.erase(at,count);}
 String substring(int from,int to=-1)const{return String(s.substr(from,to<0?std::string::npos:to-from));}
 String&operator+=(char c){s+=c;return *this;}String operator+(const String&o)const{return String(s+o.s);}String operator+(const char*p)const{return String(s+p);}
};
struct Theme{unsigned bg=0,panel=1,selbg=2,accent=3,dim=4,text=5;};Theme&TH(){static Theme t;return t;}
struct Screen{bool full=false;int imageX=-1,imageY=-1;int w=320,x=0,y=0,size=1;int width(){return w;}int height(){return w==320?240:320;}
 void bounds(int a,int b,int ww,int hh){assert(a>=0&&a+ww<=w&&b>=(full?0:w==320?36:44)&&b+hh<=height()-(full?0:24));}
 void fillScreen(unsigned){}void startWrite(){}void endWrite(){}
 void pushImage(int a,int b,int w,int h,uint16_t*){bounds(a,b,w,h);imageX=a;imageY=b;}void fillRect(int x,int y,int w,int h,unsigned){bounds(x,y,w,h);}
 void fillRoundRect(int x,int y,int w,int h,int,unsigned){bounds(x,y,w,h);}void drawRoundRect(int x,int y,int w,int h,int,unsigned){bounds(x,y,w,h);}
 void drawRect(int x,int y,int w,int h,unsigned){bounds(x,y,w,h);}
 void drawLine(int x,int y,int xx,int yy,unsigned){bounds(std::min(x,xx),std::min(y,yy),abs(xx-x)+1,abs(yy-y)+1);}
 void setTextSize(int n){size=n;}void setTextColor(unsigned,unsigned){}void setCursor(int a,int b){x=a;y=b;bounds(x,y,0,size*8);}
 void print(const char*s){x+=strlen(s)*size*6;assert(x<=w);}void print(const String&s){print(s.c_str());}
 template<class T,std::enable_if_t<std::is_arithmetic_v<T>,int> =0>void print(T n){print(std::to_string(n).c_str());}
 template<class...T>void printf(const char*f,T...n){char b[180];snprintf(b,sizeof(b),f,n...);print(b);}
}tft;
void drawHeader(const char*){}void drawFooter(const char*,const char*){}void clearBody(){}
struct Button{bool state=false,edge=false,pressedEdge=false;bool down(){return state;}bool pressed(){return edge||pressedEdge;}}bUp,bDown,bLeft,bRight,bOk,bBack,bFire;
struct {int x=0,y=0;bool click=false,left=false,visible=true;}usbPointer;
bool usbInputHit(int x,int y,int w,int h){return usbPointer.click&&usbPointer.x>=x&&usbPointer.x<x+w&&usbPointer.y>=y&&usbPointer.y<y+h;}
template<class T>T constrain(T x,T lo,T hi){return std::max(lo,std::min(hi,x));}
void edges(){bUp=bDown=bLeft=bRight=bOk=bBack=bFire={};usbPointer.click=false;}
unsigned compactRequests=0;void gfxSetCompactUi(bool b){assert(b);compactRequests++;}unsigned usbInputHeld(){return 0;}
uint32_t millis(){return 0;}void usbInputTextMode(bool){}int usbInputCharacter(){return -1;}
void loadNote(){}void saveNote(){}
bool validClock=true,saveClock=true;unsigned syncs=0;
bool rtcGetLocal(tm&t){t={};t.tm_year=126;t.tm_mon=9;t.tm_mday=27;t.tm_hour=23;t.tm_min=59;t.tm_sec=59;return validClock;}
bool rtcSetLocal(int,int,int,int,int,int){return saveClock;}void rtcSyncRequest(){syncs++;}
#define TFT_RED 0xf800
#define TFT_BLACK 0
#define TFT_WHITE 0xffff
#define TFT_DARKGREY 0x7bef
'''
appsource=(a/'apps.cpp').read_text();entry=appsource[appsource.index('void enterApp(AppId next)'):]
expr=entry[entry.index('gfxSetCompactUi(')+len('gfxSetCompactUi('):entry.index(');',entry.index('gfxSetCompactUi('))]
h=(a/'apps.h').read_text();enum=h[h.index('enum AppId'):h.index('};')+2]
pre+=enum+'\nstatic bool nativeApp(AppId next){return '+expr+';}\n#include "ui_canvas.h"\n'
cases={}
s=(a/'app_calc.cpp').read_text();cases['calc']=(s[s.index('static int32_t A'):],r'''
int main(){assert(nativeApp(APP_CALC)&&nativeApp(APP_NOTE)&&nativeApp(APP_CLOCK)&&nativeApp(APP_PAINT)&&nativeApp(APP_CAMERA)&&nativeApp(APP_FILES)&&nativeApp(APP_BROWSER));for(int w:{320,480}){tft.w=w;A=B=R=op=field=0;calcEnter();A=INT32_MIN;B=INT32_MAX;compute();drawCalc();A=4;B=3;compute();
 field=0;bUp.edge=true;calcLoop(0);edges();assert(A==5);field=1;bOk.edge=true;calcLoop(1);edges();assert(op==1);
 if(w==320){usbPointer={270,80,true};calcLoop(2);assert(field==3);edges();}}
 puts("PASS: actual Calculator native/legacy boxes, large numbers, fields and pointer action");}
''')
s=(a/'app_note.cpp').read_text();g=s[s.index('static String noteText'):s.index('static void loadNote(')];body=g+''.join(fn(s,n) for n in ['static void ensureCursor(','static char getCharAtCursor(','static void setCharAtCursor(','static void drawNote(','void noteEnter(','void noteLoop(','void noteExit(']);cases['note']=(body,r'''
int main(){for(int w:{320,480}){tft.w=w;noteText=String(std::string(240,'N'));noteEnter();assert(cursor==240&&viewStart>0);drawNote();
 bLeft.edge=true;noteLoop(0);edges();assert(cursor==239);bRight.edge=true;noteLoop(1);edges();assert(cursor==240);
 if(w==320){usbPointer={24,92,true};int top=viewStart;noteLoop(2);edges();assert(cursor==top);}
 noteExit();}puts("PASS: actual Note native/legacy text viewport, 240-char scrolling and pointer cursor");}
''')
s=(a/'app_clock.cpp').read_text();cases['clock']=(s[s.index('static uint32_t lastDraw'):],r'''
int main(){for(int w:{320,480}){tft.w=w;clockEnter();bOk.edge=true;clockLoop(0);edges();assert(editing);drawClock();
 if(w==320){usbPointer={230,150,true};clockLoop(1);edges();assert(field==5);}
 saveClock=false;bOk.edge=true;clockLoop(2);edges();assert(editing);saveClock=true;bOk.edge=true;clockLoop(3);edges();assert(!editing);
 validClock=false;clockEnter();validClock=true;bUp.edge=true;clockLoop(4);edges();}
 assert(syncs==2);puts("PASS: actual Clock native/legacy date/time, six edit boxes, pointer selection, invalid save and NTP");}
''')
s=(a/'app_paint.cpp').read_text();cases['paint']=(s[s.index('static constexpr int CW'):],r'''
int main(){for(int w:{320,480}){tft.w=w;paintEnter();for(int i=0;i<48;i++){bRight.edge=true;paintLoop(i);edges();}bOk.state=true;paintLoop(100);bOk.state=false;paintLoop(700);paintEnter();}
 puts("PASS: actual Paint native/legacy canvas, cell/cursor bounds and color indicator");}
''')
s=(r/'OS/freslest/camera.cpp').read_text();cases['camera']=(r'''static int drawX=0,drawY=0;static unsigned frameLen=20;static uint8_t jpgBuf[20]{};
bool tft_output(int16_t,int16_t,uint16_t,uint16_t,uint16_t*);
constexpr int JDR_OK=0;
struct Decoder{unsigned scale=1,used=0;bool valid=true;int getJpgSize(uint16_t*w,uint16_t*h,const uint8_t*,unsigned){*w=640;*h=480;return valid?0:1;}
 void setJpgScale(unsigned s){scale=s;}void drawJpg(int x,int y,const uint8_t*,unsigned){uint16_t b[256]{};used=scale;assert(tft_output(x,y,16,16,b));}
}TJpgDec;
'''+''.join(fn(s,n) for n in ['bool tft_output(','static void cameraDrawStatus(','static void cameraDrawBar(','static bool cameraDecodeFrame(']),r'''
int main(){tft.full=true;uint16_t pixels[16*16]{};for(int w:{320,480}){tft.w=w;assert(tft_output(12,30,16,16,pixels));assert(tft.imageX==12&&tft.imageY==30);cameraDrawStatus("Connecting WiFi...","Please wait");cameraDrawBar();assert(cameraDecodeFrame());assert(TJpgDec.used==(w==320?4u:2u)&&TJpgDec.scale==1);TJpgDec.valid=false;assert(!cameraDecodeFrame());TJpgDec.valid=true;}
 puts("PASS: actual Camera text/bar and JPEG tile coordinates on native/legacy canvas");}
''')
s=(a/'text_entry.cpp').read_text();body=__import__('re').sub(r'^#include[^\n]*\n','',s,flags=__import__('re').MULTILINE);cases['editor']=('#include "text_entry.h"\n'+body,r'''
int main(){for(int w:{320,480}){tft.w=w;textEntryBegin("Edit",std::string(240,'X').c_str(),240,false);assert(textEntryActive());textEntryLoop();bRight.edge=true;textEntryLoop();edges();usbPointer={uiCanvasX(326),uiCanvasY(254),true};textEntryLoop();edges();assert(!textEntryActive());char out[241];assert(textEntryTakeResult(out,sizeof(out))==TextEntryResult::Accepted&&strlen(out)==240);}
 assert(compactRequests==2);puts("PASS: actual screen keyboard native/legacy text viewport, key/action bounds and cancellation");}
''')
with tempfile.TemporaryDirectory() as d:
 for name,(body,test) in cases.items():
  if len(__import__('sys').argv)>1 and name not in __import__('sys').argv[1:]:continue
  p=Path(d)/(name+'.cpp');p.write_text(pre+body+test);exe=p.with_suffix('')
  subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror=misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(a),str(p),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
