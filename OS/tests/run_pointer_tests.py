from pathlib import Path
import os,subprocess
base=Path(__file__).resolve().parent;build=base/'host_build';build.mkdir(exist_ok=True)
s=(base.parent/'freslest/gfx.cpp').read_text();start=s.index('void gfxPresent()');brace=s.index('{',start);end=brace+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
pre=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>
#include "freslest_cursor.h"
#include "freslest_video.h"
void gfxMirrorTick(){}
static bool composite;static unsigned compositePresents;
freslest_display_t freslest_video_output(){return composite?FRESLEST_DISPLAY_COMPOSITE:FRESLEST_DISPLAY_LCD;}
void freslest_video_rgb565(const uint16_t* p,int w,int h,bool swapped,bool cursor,int x,int y){assert(p&&w==480&&h==360);(void)swapped;(void)cursor;(void)x;(void)y;compositePresents++;}
struct Pointer{int x=240,y=160;bool visible=false;}usbPointer;
static uint16_t buffer[480*320],shadow[480*320],lcd[480*320];static uint16_t* presented=shadow;
static bool firstPresent=true,oldPointer=false;static int oldX=0,oldY=0;
struct Device{
 void fillRect(int x,int y,int w,int h,unsigned c){for(int yy=std::max(y,0);yy<std::min(y+h,320);yy++)for(int xx=std::max(x,0);xx<std::min(x+w,480);xx++)lcd[yy*480+xx]=c;}
 std::vector<std::pair<int,int>> pending;
 int top=0,height=320;unsigned rows=0;void startWrite(){}void endWrite(){assert(pending.empty());}
 void waitDMA(){for(auto [y,h]:pending){
  if(usbPointer.visible&&usbPointer.y>=y&&usbPointer.y<y+h)assert(buffer[usbPointer.y*480+usbPointer.x]==0);
  memcpy(lcd+y*480,buffer+y*480,h*960);
 }pending.clear();}
 void setClipRect(int,int y,int,int h){top=y;height=h;}void clearClipRect(){top=0;height=320;}
 void fillTriangle(int x,int y,int,int,int,int,unsigned c){for(int yy=std::max(y,0);yy<std::min(y+12,320);yy++)for(int xx=std::max(x,0);xx<std::min(x+6,480);xx++)lcd[yy*480+xx]=c;}
}_tft;
struct Frame{int height(){return composite?360:320;}void* getBuffer(){return buffer;}void pushSprite(Device* d,int,int){d->pending.push_back({d->top,d->height});d->rows+=d->height;}}frame;
'''
test=r'''
int main(){
 for(int i=0;i<480*320;i++)buffer[i]=i;
 gfxPresent();assert(_tft.rows==320&&!memcmp(lcd,buffer,sizeof(lcd)));_tft.rows=0;gfxPresent();assert(!_tft.rows);
 buffer[42*480+50]=0x1234;gfxPresent();assert(_tft.rows==1&&lcd[42*480+50]==0x1234);
 usbPointer.visible=true;gfxPresent();assert(lcd[160*480+240]!=buffer[160*480+240]);
 _tft.rows=0;for(int i=0;i<200;i++)gfxPresent();assert(_tft.rows==0);
 assert(buffer[160*480+240]==(uint16_t)(160*480+240));usbPointer.x=300;usbPointer.y=190;gfxPresent();for(int y=160;y<179;y++)assert(!memcmp(lcd+y*480,buffer+y*480,960));
 _tft.rows=0;usbPointer.x=310;gfxPresent();assert(_tft.rows==19&&lcd[190*480+300]==buffer[190*480+300]);
 usbPointer.x=300;gfxPresent();buffer[190*480+300]=0xa55a;gfxPresent();assert(lcd[190*480+300]==0&&buffer[190*480+300]==0xa55a);usbPointer.visible=false;gfxPresent();assert(!memcmp(lcd,buffer,sizeof(lcd)));usbPointer={479,319,true};gfxPresent();usbPointer.visible=false;gfxPresent();assert(!memcmp(lcd,buffer,sizeof(lcd)));
 composite=true;auto rows=_tft.rows;gfxPresent();assert(compositePresents==1&&rows==_tft.rows);
 puts("PASS: actual LCD pointer is composed before SPI DMA, has no background-only erase, stationary pointer sends no pixels, base restored after DMA, horizontal/vertical movement, unplug, redraw and edge clipping");
}
'''
p=build/'pointer_actual.cpp';p.write_text(pre+s[s.index('static __attribute__((noinline,noclone)) void gfxCursor('):end]+test);exe=build/'test_pointer'
subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(base.parent.parent/'shared_components/freslest_video/include'),str(p),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
