#include <HardwareSerial.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "audio_manager.h"
uint32_t testMillis=0;
std::vector<std::array<uint8_t,10>> testPackets;
int soundVolume=180;
extern bool soundEnabled;
static void advance(unsigned n){for(unsigned i=0;i<n;++i)audioUpdate(++testMillis);}
static void reply(uint8_t command,uint16_t value){
 std::array<uint8_t,10> p={0x7e,0xff,6,command,0,(uint8_t)(value>>8),(uint8_t)value,0,0,0xef};
 uint16_t sum=0;for(int i=1;i<=6;++i)sum+=p[i];sum=(uint16_t)(0-sum);p[7]=sum>>8;p[8]=sum;
 for(auto b:p)testSerialRx.push_back(b);
 advance(1);
}
static unsigned count(uint8_t cmd,size_t start=0){unsigned n=0;for(size_t i=start;i<testPackets.size();++i)n+=testPackets[i][3]==cmd;return n;}
static int lastVolume(){for(auto i=testPackets.rbegin();i!=testPackets.rend();++i)if((*i)[3]==6)return (*i)[6];return -1;}
int main(int argc,char**argv){
 assert(argc==2);const char* name=argv[1];
 if(!strcmp(name,"init_volume")){
   soundEnabled=false;audioInit();
   for(unsigned n=0;n<6000&&lastVolume()<0;++n)advance(1);
   assert(lastVolume()==0&&!audioGetDiagnostics().ready);
   soundEnabled=true;soundVolume=255;soundApply();advance(1500);
   assert(audioGetDiagnostics().ready&&lastVolume()==30);
   audioClick();advance(150);assert(audioGetDiagnostics().lastTrack==106);
 }else{
   audioInit();advance(6000);assert(audioGetDiagnostics().ready);
   if(!strcmp(name,"sd_query")){
     assert(count(0x48)>0&&count(0x47)==0);
     reply(0x48,112);reply(0x47,0);
     assert(audioGetDiagnostics().cardPresent&&audioGetDiagnostics().fileCount==112);
   }else if(!strcmp(name,"lost_insert")){
     reply(0x48,112);reply(0x3b,2);assert(!audioGetDiagnostics().ready);
     auto first=testPackets.size();advance(2400);assert(count(0x48,first)>0);
     reply(0x48,112);advance(2500);assert(audioGetDiagnostics().ready&&audioGetDiagnostics().recoveries==1);
     assert(count(0x0c)==1&&count(0x12,first)==0);
   }else if(!strcmp(name,"latest_volume")){
     for(unsigned n=0;n<40;++n){soundVolume=0;soundApply();}
     soundEnabled=true;soundVolume=255;soundApply();advance(5000);assert(lastVolume()==30);
   }else if(!strcmp(name,"sleep")){
     auto first=testPackets.size();reply(0x40,2);advance(3000);
     assert(count(0x0b,first)==1&&count(0x09,first)==1&&count(0x1a,first)==1);
     assert(audioGetDiagnostics().ready&&lastVolume()==21&&count(0x0c)==1&&count(0x12,first)==0);
   }else if(!strcmp(name,"runtime_volume")){
     reply(0x43,0);advance(300);assert(lastVolume()==21&&count(6)>=2);
   }else if(!strcmp(name,"silent_rx")){
     auto first=testPackets.size();advance(30000);assert(count(0x0c)==1&&count(0x48,first)==0&&audioGetDiagnostics().recoveries==0);
   }else assert(false);
 }
 std::printf("PASS: %s\n",name);
}
