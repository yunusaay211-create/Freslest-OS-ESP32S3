"""Render actual Wi-Fi pages on the native canvas and check text/control bounds."""
from pathlib import Path
import os, subprocess, tempfile
r=Path(__file__).resolve().parents[1];app=r/'OS/freslest/src/freslest'
def function(s,name):
 start=s.index(name);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
s=(app/'wifi_manager.cpp').read_text()
pre=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdint>
#define FRESLEST_LCD_MODEL 9341
struct Screen {
 int x=0,y=0;int width(){return 320;}
 void fillRect(int,int,int,int,unsigned){}void drawRoundRect(int x,int y,int w,int h,int r,unsigned c){fillRoundRect(x,y,w,h,r,c);}
 void fillScreen(unsigned){}void setTextSize(int n){assert(n==1);}
 void setTextColor(unsigned,unsigned){}
 void setCursor(int a,int b){x=a;y=b;assert(x>=0&&y>=36&&y+8<=216);}
 void fillRoundRect(int x,int y,int w,int h,int,unsigned){assert(x>=0&&x+w<=320&&y>=36&&y+h<=216);}
 void print(const char*s){x+=strlen(s)*6;assert(x<=320);}
 template<class... A>void printf(const char*f,A... a){char b[256];snprintf(b,sizeof(b),f,a...);print(b);}
}tft;
struct Pointer{int x=0,y=0;}usbPointer;
bool usbInputHit(int,int,int,int){return false;}
#include "settings_canvas.h"
struct Theme{unsigned bg=0,text=1,dim=2,panel=3,selbg=4;};Theme& TH(){static Theme t;return t;}
void gfxSetCompactUi(bool b){assert(b);}void drawHeader(const char*){}void clearBody(){}void drawFooter(const char*,const char*){}
constexpr int WL_CONNECTED=3,WIFI_AUTH_OPEN=0;
std::string wifiSSID(32,'S');bool wifiEnabled=true,portalOn=true,scanning=false;
char wifiMessage[80]="1234567890123456789012345678901234567890123456789012345678901234567890123456789";
int row=0,networkCount=8,networkRow=7,networkTop=0;
struct IP{std::string toString(){return "255.255.255.255";}};
struct Wifi{int state=WL_CONNECTED;int status(){return state;}IP localIP(){return {};}
 int encryptionType(int){return 1;}std::string SSID(int){return std::string(32,'N');}long RSSI(int){return -100;}
}WiFi;
'''
code=pre+function(s,'void drawMain(')+function(s,'void drawNetworks(')+r'''
int main(){for(row=0;row<5;row++)drawMain();WiFi.state=0;wifiEnabled=false;drawMain();
 drawNetworks();assert(networkTop==2);scanning=true;drawNetworks();scanning=false;networkCount=0;drawNetworks();
 puts("PASS: actual native Wi-Fi main/network pages fit 320x240 with long SSIDs, portal address, status, six scrolled networks and empty/scanning states");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'wifi_ui.cpp';p.write_text(code);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(app),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
