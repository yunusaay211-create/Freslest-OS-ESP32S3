"""Exercise actual station lifecycle and status drawing with deterministic doubles."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];app=r/'OS/freslest/src/freslest'
def function(source,name):
 start=source.index(name);begin=source.index('{',start);end=begin+1;depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]
wifi=(app/'wifi_manager.cpp').read_text();status=(app/'system_status.cpp').read_text()
pre=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <tuple>
#include <ctime>
#include "ble_advertising.h"
constexpr int WL_CONNECTED=3,WL_CONNECT_FAILED=4,WIFI_SCAN_RUNNING=-1,WIFI_STA=1,APP_CAMERA=8;
unsigned fakeNow=100,saves=0,cancels=0;int app=0;
unsigned millis(){return fakeNow;}
size_t strlcpy(char*d,const char*s,size_t n){size_t l=strlen(s);memcpy(d,s,l<n?l:n-1);d[l<n?l:n-1]=0;return l;}
bool wifiEnabled=true;std::string wifiSSID="first",wifiPASS="password";
struct Wifi {
 int state=0,scan=0,begins=0,disconnects=0,apOff=0,staOff=0;std::string ssid;
 void persistent(bool p){assert(!p);}void setAutoReconnect(bool p){assert(!p);}void mode(int m){assert(m==WIFI_STA);}
 bool staSuccess=true,beginSuccess=true;bool enableSTA(bool on){if(!on)staOff++;return staSuccess;}void enableAP(bool on){assert(!on);apOff++;}
 void disconnect(bool off,bool erase){assert(!off&&!erase);disconnects++;state=0;}
 int begin(const char*s,const char*){begins++;ssid=s;state=0;return beginSuccess?0:WL_CONNECT_FAILED;}int status(){return state;}int scanComplete(){return scan;}
}WiFi;
struct Portal {unsigned handled=0,stops=0;void handleClient(){handled++;}void stop(){stops++;}}portal;
bool portalOn=false,begun=false,requested=true,lastEnabled=false,attempting=false,wasConnected=false;
bool startupAllowed=false;unsigned errors=0;enum class FreslestBootStage{Wifi};void freslestSafetyStage(FreslestBootStage){}void freslestSafetyError(const char*){++errors;}
unsigned connectAt=0;int editing=0;constexpr int NoEdit=0;
void settingsSave(){saves++;}void textEntryCancel(){cancels++;}void message(const char*){}
void wifiManagerBegin();
struct Theme{unsigned panel=1,text=2,accent=3,dim=4;};Theme& TH(){static Theme t;return t;}
struct Screen {
 std::vector<std::tuple<int,int,int,int>> rects;std::vector<std::tuple<int,int,std::string>> words;
 int w=480;int width(){return w;}int x=0,y=0;
 void fillRect(int a,int b,int c,int d,unsigned){rects.emplace_back(a,b,c,d);}
 void setTextSize(int n){assert(n==1);}void setTextColor(unsigned,unsigned){}
 void setCursor(int a,int b){x=a;y=b;}void print(const char*s){words.emplace_back(x,y,s);}
 void printf(const char*fmt,const char*s){char b[32];snprintf(b,sizeof(b),fmt,s);print(b);}
 void drawLine(int a,int b,int c,int d,unsigned){assert(a>=345&&a<=387&&c>=345&&c<=387&&b>=4&&d<=21);}
 void fillCircle(int a,int b,int,unsigned){assert(a==354&&b==18);}
}tft;
static char previous[48]="";static unsigned statusDrawAt=0;
bool rtcGetLocal(tm&t){t={};t.tm_hour=23;t.tm_min=15;t.tm_sec=30;return true;}
bool dhtReading(float&t,float&h){t=24;h=50;return true;}
FreslestBleInfo freslestBleInfo(){FreslestBleInfo b{};b.enabled=true;b.connected=1;return b;}
'''
code=pre+'\n'.join(function(wifi,n) for n in ['void beginConnect(','void stopPortal(','void wifiManagerBegin(','void wifiManagerAllowStartup(','void wifiManagerRequest(','void wifiManagerSetEnabled(','bool wifiPortalActive(','void wifiManagerUpdate(','void setWifiExit('])+function(status,'void systemStatusDraw(')
test=r'''
int main(){
 wifiManagerBegin();wifiManagerUpdate(100);assert(WiFi.begins==0);wifiManagerAllowStartup();wifiManagerUpdate(101);assert(WiFi.begins==1&&WiFi.ssid=="first");WiFi.state=WL_CONNECTED;wifiManagerUpdate(200);assert(!attempting);
 unsigned disconnects=WiFi.disconnects;setWifiExit();assert(WiFi.disconnects==(int)disconnects&&WiFi.state==WL_CONNECTED&&saves==1&&cancels==1);
 portalOn=true;stopPortal();assert(!portalOn&&WiFi.apOff==1&&WiFi.state==WL_CONNECTED&&WiFi.begins==1);
 wifiSSID="second";wifiManagerRequest();wifiManagerUpdate(300);assert(WiFi.begins==2&&WiFi.ssid=="second");
 WiFi.state=WL_CONNECTED;wifiManagerUpdate(400);WiFi.state=0;wifiManagerUpdate(500);wifiManagerUpdate(6499);assert(WiFi.begins==2);wifiManagerUpdate(6500);assert(WiFi.begins==3);
 wifiManagerSetEnabled(false);wifiManagerUpdate(50000);assert(!wifiEnabled&&WiFi.staOff==1&&WiFi.begins==3&&wifiSSID=="second");
 wifiManagerSetEnabled(true);app=APP_CAMERA;wifiManagerUpdate(51000);assert(WiFi.begins==3);app=0;wifiManagerUpdate(52000);assert(WiFi.begins==4);
 WiFi.scan=WIFI_SCAN_RUNNING;wifiManagerRequest();wifiManagerUpdate(60000);assert(WiFi.begins==4);WiFi.scan=0;wifiManagerUpdate(60001);assert(WiFi.begins==5);
 WiFi.staSuccess=false;wifiManagerRequest();wifiManagerUpdate(61000);assert(errors==1&&WiFi.begins==5);wifiManagerUpdate(80999);assert(errors==1);WiFi.staSuccess=true;WiFi.beginSuccess=false;wifiManagerUpdate(81000);assert(errors==2&&WiFi.begins==6);WiFi.beginSuccess=true;
 wifiPASS=std::string(65,'x');wifiManagerRequest();wifiManagerUpdate(82000);assert(errors==3&&WiFi.begins==6);wifiPASS="password";
 WiFi.state=WL_CONNECTED;systemStatusDraw(true);assert(tft.rects.size()==1&&std::get<0>(tft.rects[0])==116&&std::get<0>(tft.rects[0])+std::get<2>(tft.rects[0])==396);
 assert(std::get<0>(tft.words[0])==120&&std::get<2>(tft.words[0])=="23:15:30"&&std::get<0>(tft.words[1])==180&&std::get<2>(tft.words[1])=="24.0 C");
 unsigned count=tft.rects.size();fakeNow+=300;systemStatusDraw(false);assert(tft.rects.size()==count);
 puts("PASS: actual Wi-Fi manager: leaving Settings and stopping portal preserve station, changed credentials replace active connection, delayed boot startup, failed STA/begin and invalid-credential handling, bounded retry, explicit off retains credentials, Camera ownership, scan coordination; actual header time/temperature beside FRESLEST and Wi-Fi/BT beside battery without overlap.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'wifi.cpp';p.write_text(code+test);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(app),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
