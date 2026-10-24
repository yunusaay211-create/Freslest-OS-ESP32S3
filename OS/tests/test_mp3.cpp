#include <HardwareSerial.h>
#include <cassert>
#include <cstdio>
#include "audio_manager.h"
uint32_t testMillis=0;
std::vector<std::array<uint8_t,10>> testPackets;
int soundVolume=180;
extern bool soundEnabled;
void advance(unsigned n){for(unsigned i=0;i<n;i++){++testMillis;audioUpdate(testMillis);}}
int main(){
 audioInit();audioInit();audioBootMelody();advance(999);assert(testPackets.size()==1&&testPackets[0][3]==0x16);advance(4501);assert(audioGetDiagnostics().ready);
 bool boot=false,source=false,stopped=false;for(auto& p:testPackets){unsigned sum=0;for(int i=1;i<7;i++)sum+=p[i];assert((uint16_t)(sum+(p[7]<<8)+p[8])==0);assert(p[0]==0x7e&&p[9]==0xef);if(p[3]==0x12&&p[6]==105)boot=true;if(p[3]==9&&p[6]==2)source=true;if(p[3]==0x16)stopped=true;}assert(boot&&source&&stopped);
 auto size=testPackets.size();audioClick();advance(100);assert(testPackets.size()==size);advance(1500);audioClick();advance(200);assert(testPackets.back()[3]==0x12&&testPackets.back()[6]==106);
 audioGameOver();advance(250);assert(testPackets.back()[6]==111);size=testPackets.size();audioClick();advance(250);assert(testPackets.size()==size);
 soundEnabled=false;soundApply();advance(250);assert(testPackets.back()[3]==0x06&&testPackets.back()[6]==0);size=testPackets.size();audioWin();advance(250);assert(testPackets.size()==size);
 soundEnabled=true;soundVolume=255;soundApply();advance(250);assert(testPackets.back()[3]==6&&testPackets.back()[6]==30);audioWin();advance(250);assert(testPackets.back()[6]==112);
 bool reset=false,dac=false;for(auto& p:testPackets){if(p[3]==0x0c)reset=true;if(p[3]==0x1a&&p[6]==0)dac=true;}assert(reset&&dac);
 auto reply=[](uint8_t command,uint16_t value,bool valid){std::array<uint8_t,10> p={0x7e,0xff,6,command,0,(uint8_t)(value>>8),(uint8_t)value,0,0,0xef};uint16_t sum=0;for(int i=1;i<=6;i++)sum+=p[i];sum=(uint16_t)(0-sum);p[7]=sum>>8;p[8]=(uint8_t)sum;if(!valid)p[8]^=1;for(auto b:p)testSerialRx.push_back(b);};
 testSerialRx.push_back(0);reply(0x48,179,false);reply(0x48,179,true);advance(1);auto status=audioGetDiagnostics();assert(status.replySeen&&status.cardPresent&&status.fileCount==179&&status.invalidPackets);
 reply(0x40,6,true);advance(1);assert(audioGetDiagnostics().lastError==6);reply(0x3b,2,true);advance(1);assert(!audioGetDiagnostics().cardPresent);
 void(*cues[])()={audioBoot,audioClick,audioDino,audioSnake,audioTetris,audioGame,audioGameOver,audioWin};
 for(unsigned i=0;i<8;++i){advance(2000);cues[i]();advance(300);assert(audioGetDiagnostics().lastTrack==105+i);}
 for(const auto& packet:testPackets)if(packet[3]==0x12){unsigned track=(packet[5]<<8)|packet[6];assert(track>=105&&track<=112);}
 puts("PASS: OS MP3 packets/checksums, reset/SD/DAC sequence, boot priority, mute, volume, no-feedback playback, UART reply/SD/error parsing and corrupt packet recovery");
 puts("PASS: all eight OS events address only /MP3/0105..0112 with MP3-folder playback; no Doom file or ADVERT selection");
}
