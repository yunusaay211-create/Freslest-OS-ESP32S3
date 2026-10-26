"""Run the real Bluetooth settings page with asynchronous host API doubles."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];a=r/'OS/freslest/src/freslest'
s=(a/'settings_pages.cpp').read_text();code=s[s.index('static void bleStart()'):s.index('// ===== Flash Reset =====')]
pre=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "ble_advertising.h"
static FreslestBleInfo info{};static int starts,stops,scans,connects,forgot,replyYes,replyNo,saves;
FreslestBleInfo freslestBleInfo(){return info;}
void freslestBleStart(){starts++;info.enabled=true;info.revision++;}
void freslestBleStop(){stops++;info.enabled=false;info.revision++;}
void freslestBleScan(){scans++;}
void freslestBleConnect(unsigned i){assert(i<info.count);connects++;}
void freslestBleForget(){forgot++;}
void freslestBlePairReply(bool v){if(v)replyYes++;else replyNo++;info.pairMode=0;info.revision++;}
bool btEnabled=false;
void settingsSave(){saves++;}
struct Button{bool edge=false;bool pressed(){return edge;}}bUp,bDown,bLeft,bRight,bOk,bFire;
struct {int x=0,y=0;bool click=false;}usbPointer;
bool usbInputHit(int x,int y,int w,int h){return usbPointer.click&&usbPointer.x>=x&&usbPointer.x<x+w&&usbPointer.y>=y&&usbPointer.y<y+h;}
struct Theme{unsigned bg=0,text=1,selbg=2,dim=3;};Theme& TH(){static Theme t;return t;}
struct Display {
 std::vector<std::string> text;int h=320;int width(){return 480;}
 void fillScreen(unsigned){text.clear();}void setTextSize(unsigned){}void setTextColor(unsigned,unsigned){}
 void setCursor(int x,int y){assert(x>=0&&x<480&&y>=44&&y<h-24);}
 void fillRoundRect(int,int y,int,int h1,int,unsigned){assert(y>=44&&y+h1<=h-24);}
 void print(const char*s){text.emplace_back(s);}void printf(const char*,...){}
}tft;
void drawHeader(const char*){}void clearBody(){}void drawFooter(const char*,const char*){}
void edges(){bUp=bDown=bLeft=bRight=bOk=bFire={};usbPointer.click=false;}
'''
test=r'''
int main(){
 setBtEnter();assert(stops==1&&!btEnabled);
 bRight.edge=true;setBtLoop(0);edges();assert(starts==1&&saves==1&&btEnabled);
 info.count=2;strcpy(info.devices[0].name,"BLE keyboard");strcpy(info.devices[1].name,"BLE mouse");info.revision++;
 setBtLoop(1);assert(btRow==0);bOk.edge=true;setBtLoop(2);edges();assert(connects==1);
 bDown.edge=true;setBtLoop(3);edges();assert(btRow==1);bOk.edge=true;setBtLoop(4);edges();assert(connects==2);
 bFire.edge=true;setBtLoop(5);edges();assert(scans==1);
 bDown.edge=true;setBtLoop(6);edges();bOk.edge=true;setBtLoop(7);edges();assert(btForgetConfirm&&!forgot);
 bLeft.edge=true;setBtLoop(8);edges();assert(!btForgetConfirm&&!forgot&&btEnabled);
 bOk.edge=true;setBtLoop(9);edges();bOk.edge=true;setBtLoop(10);edges();assert(forgot==1&&!btForgetConfirm);
 info.pairMode=1;info.passkey=123456;info.revision++;setBtLoop(11);bOk.edge=true;setBtLoop(12);edges();assert(!replyYes);
 info.pairMode=2;info.revision++;setBtLoop(13);bOk.edge=true;setBtLoop(14);edges();assert(replyYes==1);
 info.count=6;info.revision++;tft.h=360;setBtLoop(15);
 usbPointer={25,91+3*23+6,true};setBtLoop(16);edges();assert(btRow==3&&connects==3);
 bLeft.edge=true;setBtLoop(17);edges();assert(!btEnabled&&stops==2&&saves==2);
 puts("PASS: actual BLE settings ON/OFF/save, asynchronous list redraw, separate keyboard/mouse selection, scan, confirmed forget/cancel, passkey display/explicit numeric confirmation, pointer selection and 320/360-height bounds");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'bt.cpp';p.write_text(pre+s[s.index("// Compact settings adapter"):s.index("// ===== Settings menu =====")]+code+test);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(a),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
