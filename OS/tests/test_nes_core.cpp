#include <cassert>
#include <cstdio>
#include <cstring>
#include <LovyanGFX.hpp>
#include "nes_core.h"
#include "nes_audio.h"
#include "mock_audio.h"
#include <vector>
#include <cmath>
#include <algorithm>
#define FRESLEST_INPUT_HOST_TEST 1
#include "../freslest/input.cpp"
uint32_t testMillis=0;
void audioClick(){}
extern "C" {
#include "noftypes.h"
#include "nes/nes_ppu.h"
#include "osd.h"
}
bool soundEnabled=true;
int soundVolume=220;
uint16_t testPixels[480*360];
static uint16_t firstRunPixels[480*360];
static lgfx::LGFX_Device device;
static unsigned pushedRows=0;
lgfx::LGFXBase& tft=device;
void lgfx::LGFX_Device::pushImage(int x,int y,int w,int h,const uint16_t* p) {
  ++pushedRows;
  assert(x>=0&&y>=0&&x+w<=480&&y+h<=device.height());
  for(int j=0;j<h;j++)memcpy(testPixels+(y+j)*480+x,p+j*w,w*2);
}
static void* allocations[8192];
extern "C" void* __real_malloc(size_t);
extern "C" void __real_free(void*);
extern "C" void* __wrap_malloc(size_t n) {
 void* p=__real_malloc(n);if(!p)return p;
 for(auto& a:allocations)if(!a){a=p;return p;}
 assert(false);return nullptr;
}
extern "C" void __wrap_free(void* p) {
 if(p)for(auto& a:allocations)if(a==p){a=nullptr;break;}
 __real_free(p);
}
static unsigned allocated() {unsigned n=0;for(auto p:allocations)if(p)++n;return n;}
int main(int argc,char** argv) {
  assert(argc==2);
  assert(!nesCoreStart("/not-a-rom.nes"));assert(nesCoreMapperSupported(0));assert(!nesCoreMapperSupported(255));
  unsigned baseline=allocated();
  std::vector<uint16_t> actualAudio;
  for(int run=0;run<40;run++) {
    assert(nesCoreStart(argv[1]));
    unsigned phase=0;mockDuties.clear();
    for(int frame=0;frame<(run==0?600:4);frame++) {
      uint8_t pad=run==0?(frame>70&&frame<73?0x08:frame>100?0x81:0):(frame<3?(uint8_t)(1u<<(run%8)):0);
      nesCoreFrame(frame%2==0,pad);
      // Read the actual emulated controller port, after the real CPU frame.
      // A picture/audio-only test cannot prove that a game receives buttons.
      ppu_writehigh(0x4016,1);ppu_writehigh(0x4016,0);
      uint8_t serialPad=0;
      for(unsigned bit=0;bit<8;++bit)serialPad|=(ppu_readhigh(0x4016)&1u)<<bit;
      assert(serialPad==pad);
      phase+=8000;unsigned count=phase/60;phase%=60;mockAudioTick(count);
    }
    assert(nesAudioStats().dropped==0&&nesAudioStats().underruns==0);
    if(run==0)actualAudio=mockDuties;
    if(run==0)memcpy(firstRunPixels,testPixels,sizeof(testPixels));
    nesCoreStop();assert(allocated()==baseline);
  }
  // Physical GPIO levels -> real sampler -> pad -> real emulated $4016.
  for(auto& pin:testPins)pin=HIGH;
  inputBegin();samplerStarted=true;inputNesBegin();assert(inputNesPad()==0);
  assert(nesCoreStart(argv[1]));
  auto sampleKeys=[](uint8_t keys){
    for(unsigned i=0;i<7;++i)testPins[pins[i]]=(keys&(1u<<i))?LOW:HIGH;
    for(int i=0;i<30;i+=2){testMillis+=2;sampleButtons(testMillis);}
  };
  auto checkPort=[](uint8_t expected){
    nesCoreFrame(false,inputNesPad());mockAudioTick(133);
    ppu_writehigh(0x4016,1);ppu_writehigh(0x4016,0);uint8_t mask=0;
    for(int bit=0;bit<8;++bit)mask|=(ppu_readhigh(0x4016)&1)<<bit;
    assert(mask==expected);
  };
  const uint8_t keys[]={KeyOk,KeyFire,KeyDown|KeyOk,KeyUp|KeyOk,KeyUp,KeyDown,KeyLeft,KeyRight};
  for(unsigned i=0;i<8;++i){sampleKeys(keys[i]);checkPort(1u<<i);sampleKeys(0);checkPort(0);}
  nesCoreStop();assert(allocated()==baseline);
  puts("PASS: actual GPIO -> debounce sampler -> NES pad -> CPU/controller port path for all eight buttons and releases");

  // Exercise the real rendering callback with a deterministic bitmap.
  vidinfo_t info;osd_getvideoinfo(&info);auto* driver=info.driver;
  assert(driver->init(256,240)==0);bitmap_t* picture=bmp_create(256,240,0);assert(picture);
  rgb_t colors[256];for(int i=0;i<256;++i)colors[i]={i,(i*3)&255,(255-i)};
  driver->set_palette(colors);
  for(int y=0;y<240;++y)for(int x=0;x<256;++x)picture->line[y][x]=(x+y)&255;
  nesCoreSetFullscreen(false);std::fill(std::begin(testPixels),std::end(testPixels),0xdead);pushedRows=0;
  driver->custom_blit(picture,-1,nullptr);assert(pushedRows==240);
  auto pixel=[&](int x,int y){auto c=colors[picture->line[y][x]];return (uint16_t)(((c.r>>3)<<11)|((c.g>>2)<<5)|(c.b>>3));};
  assert(testPixels[48*480+112]==pixel(0,0)&&testPixels[287*480+367]==pixel(255,239));
  pushedRows=0;driver->custom_blit(picture,-1,nullptr);assert(!pushedRows);
  picture->line[7][3]^=1;driver->custom_blit(picture,-1,nullptr);assert(pushedRows==1);
  nesCoreSetFullscreen(true);pushedRows=0;driver->custom_blit(picture,-1,nullptr);assert(pushedRows==320);
  for(int y=0;y<320;++y)for(int x=0;x<426;++x)assert(testPixels[y*480+27+x]==pixel(x*256/426,y*240/320));
  assert(testPixels[0]==0xdead&&testPixels[479]==0xdead);
  pushedRows=0;driver->custom_blit(picture,-1,nullptr);assert(!pushedRows);
  picture->line[0][0]^=1;driver->custom_blit(picture,-1,nullptr);assert(pushedRows==2);
  driver->set_palette(colors);pushedRows=0;driver->custom_blit(picture,-1,nullptr);assert(pushedRows==320);
  device.heightValue=360;nesCoreSetFullscreen(false);nesCoreSetFullscreen(true);pushedRows=0;
  driver->custom_blit(picture,-1,nullptr);assert(pushedRows==360);
  for(int y=0;y<360;++y)for(int x=0;x<480;++x)assert(testPixels[y*480+x]==pixel(x*256/480,y*240/360));
  device.heightValue=320;
  bmp_destroy(&picture);driver->shutdown();nesCoreSetFullscreen(false);assert(allocated()==baseline);
  puts("PASS: actual NES blitter skips unchanged frames, redraws one dirty source row, maps all fullscreen pixels into LCD 426x320 and full 4:3 composite 480x360, invalidates palette and frees cache");
  assert(mockCreated==mockDeleted&&actualAudio.size()==80000);
  auto low=*std::min_element(actualAudio.begin(),actualAudio.end()),high=*std::max_element(actualAudio.begin(),actualAudio.end());
  fprintf(stderr,"Real ROM PCM duty range: %u..%u\n",low,high);
  assert(high-low>20);unsigned changes=0;
  for(size_t i=1;i<actualAudio.size();i++)if(actualAudio[i]!=actualAudio[i-1])++changes;
  assert(changes>1000);
  FILE* audio=fopen("nes_actual_audio.raw","wb");for(auto duty:actualAudio) {int16_t pcm=((int)duty-512)*64;fwrite(&pcm,2,1,audio);}fclose(audio);
  printf("PASS: actual ROM -> APU -> PCM queue -> GPIO21 PWM; 80000 samples/10s, range %u..%u, %u transitions, skipped-frame audio retained\n",low,high,changes);
  memcpy(testPixels,firstRunPixels,sizeof(testPixels));
  bool varied=false;uint16_t first=testPixels[48*480+112];
  for(int y=48;y<288;y++)for(int x=112;x<368;x++)if(testPixels[y*480+x]!=first)varied=true;
  assert(varied);
  FILE* file=fopen("nes_actual_test.ppm","wb");fprintf(file,"P6\n480 320\n255\n");
  for(auto p:testPixels) {unsigned char rgb[3]={(unsigned char)((p>>11)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};fwrite(rgb,1,3,file);}fclose(file);
  puts("PASS: actual NES CPU/PPU/APU ROM frames, input, palette, pixel bounds, 40 open/exit cycles, missing ROM and mapper rejection");
  puts("PASS: actual emulated $4016 receives and releases all eight Player 1 buttons across 40 ROM restarts");
}
