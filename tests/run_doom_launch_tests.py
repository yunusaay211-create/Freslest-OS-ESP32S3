"""Run actual OS Doom launcher source against boot/audio/display API doubles."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'OS/freslest/src/freslest/app_doom.cpp').read_text()
code=s[s.index('static bool doomRequested'):]
pre=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#define ESP_OK 0
#define ESP_ERR_NOT_FOUND 1
#define ESP_ERR_OTA_VALIDATE_FAILED 2
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_OTA_2 18
#define TFT_RED 0xf800
using esp_err_t=int;
struct esp_partition_t{int address=0x20000;}partition;
struct esp_app_desc_t{char project_name[32];};
static bool ready,present=true,rightProject=true,hit;
static int descriptorError,bootError,bootCalls,descriptorCalls,restarts,stops,updates,unmounts,presents;
static std::string status;
struct {bool edge=false;bool pressed(){return edge;}}bOk;
struct Theme{int bg=0,text=1,selbg=2;};Theme& TH(){static Theme t;return t;}
struct {void fillScreen(int){}void fillRect(int,int,int,int,int){}void fillRoundRect(int,int,int,int,int,int){}
 void setTextSize(int){}void setTextColor(int,int){}void setCursor(int,int){}void print(const char*s){status=s;}void endWrite(){}}tft;
struct {void printf(const char*,...){}}Serial;
struct {void restart(){restarts++;}}ESP;
struct {void end(){unmounts++;}}LittleFS;
void drawHeader(const char*){}void clearBody(){}void drawFooter(const char*,const char*){}
unsigned millis(){return 100;}
void delay(int ms){assert(ms==110);}
void audioUpdate(unsigned){updates++;}void audioStop(){stops++;}void gfxPresent(){presents++;}
bool usbInputHit(int x,int y,int w,int h){assert(x==100&&y==210&&w==280&&h==60);bool v=hit;hit=false;return v;}
bool freslest_ota_ready(){return ready;}
const char* esp_err_to_name(esp_err_t){return "ERROR";}
const esp_partition_t* esp_partition_find_first(int type,int sub,const char*name){assert(type==0&&sub==18&&!strcmp(name,"doom"));return present?&partition:nullptr;}
esp_err_t esp_ota_get_partition_description(const esp_partition_t*p,esp_app_desc_t*d){assert(p==&partition);descriptorCalls++;std::strcpy(d->project_name,rightProject?"freslest_doom_s3":"wrong_app");return descriptorError;}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*p){assert(p==&partition);bootCalls++;return bootError;}
'''
test=r'''
int main(){
 doomEnter();doomLoop(0);assert(!bootCalls&&!doomRequested);
 bOk.edge=true;doomLoop(1);bOk.edge=false;assert(doomRequested&&!bootCalls&&status.find("Checking startup")!=std::string::npos);
 doomLoop(2);assert(!bootCalls);
 ready=true;doomLoop(3);assert(bootCalls==1&&descriptorCalls==1&&restarts==1&&presents==2&&stops==1&&updates==14&&unmounts==1&&!doomRequested);
 ready=false;doomEnter();hit=true;doomLoop(4);assert(doomRequested);doomEnter();ready=true;doomLoop(5);assert(bootCalls==1&&!doomRequested);
 present=false;bOk.edge=true;doomLoop(6);assert(bootCalls==1&&restarts==1&&status.find("partition missing")!=std::string::npos);
 present=true;rightProject=false;doomLoop(7);assert(bootCalls==1&&restarts==1);
 rightProject=true;descriptorError=9;doomLoop(8);assert(bootCalls==1);
 descriptorError=0;bootError=8;doomLoop(9);assert(bootCalls==2&&restarts==1&&stops==1);
 bootError=0;doomLoop(10);assert(bootCalls==3&&restarts==2&&stops==2);
 puts("PASS: actual OS Doom launcher queues early input, cancels on re-entry, checks image identity, reports missing/corrupt/boot errors, presents loading status and preserves audio/filesystem shutdown");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'launch.cpp';p.write_text(pre+code+test);exe=p.with_suffix('')
 subprocess.run(['g++','-std=c++17','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
