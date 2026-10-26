#include "nes_core.h"
#include "nes_audio.h"
#include "settings.h"
#include "freslest_port.h"
#include "../../gfx.h"
#include <esp_heap_caps.h>
#include <cstring>
#include <cstdio>
#include "ines_header.h"
extern "C" {
#include "noftypes.h"
#include "nofrendo.h"
#include "nes/nes.h"
#include "nes/nesinput.h"
#include "nes/nes_mmc.h"
#include "event.h"
#include "gui.h"
#include "vid_drv.h"
#include "osd.h"
void freslest_nes_prepare(void);
void freslest_nes_step(bool draw);
}
static nes_t* machine=nullptr;
static bitmap_t* screen=nullptr;
static uint8_t dummyPixel=0;
static uint16_t palette[256];
static uint8_t oldPad=0;
static uint16_t line[480];
static uint8_t* frameCache=nullptr;
static bool cacheValid=false,fullscreen=false;
void nesCoreSetFullscreen(bool enabled) {
  if(fullscreen!=enabled)cacheValid=false;
  fullscreen=enabled;
}
extern "C" void* mem_alloc(int size,bool fast) {
  if(size<=0)return nullptr;
  void* p=fast?heap_caps_malloc(size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT):heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!p)p=heap_caps_malloc(size,MALLOC_CAP_8BIT);
  return p;
}
static int videoInit(int,int) {
  screen=bmp_createhw(&dummyPixel,256,240,0);
  if(screen)frameCache=(uint8_t*)heap_caps_malloc(256*240,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  cacheValid=false;return screen?0:-1;
}
static void videoShutdown() {
  if(screen)bmp_destroy(&screen);
  if(frameCache)heap_caps_free(frameCache);
  frameCache=nullptr;cacheValid=false;
}
static int setMode(int,int) {return 0;}
static void setPalette(rgb_t* p) {for(int i=0;i<256;i++)palette[i]=((p[i].r>>3)<<11)|((p[i].g>>2)<<5)|(p[i].b>>3);cacheValid=false;}
static void clearVideo(uint8) {}
static bitmap_t* lockWrite() {return screen;}
static void freeWrite(int,rect_t*) {}
static void blit(bitmap_t* frame,int,rect_t*) {
  // Native NES, or a 4:3 fit: LCD/VGA source 426x320.
  // Only rows whose indexed pixels changed need an SPI transfer.
  bool dirty[240],any=false;
  for(int y=0;y<240;++y) {
    dirty[y]=!frameCache||!cacheValid||memcmp(frameCache+y*256,frame->line[y],256)!=0;
    any|=dirty[y];
    if(frameCache&&dirty[y])memcpy(frameCache+y*256,frame->line[y],256);
  }
  if(!any)return;
  const int height=fullscreen?tft.height():240,width=fullscreen?height*4/3:256;
  const int left=(480-width)/2,top=fullscreen?0:48;
  int previousSource=-1;
  tft.startWrite();
  for(int y=0;y<height;y++) {
    int sourceY=fullscreen?y*240/height:y;
    if(!dirty[sourceY])continue;
    if(previousSource!=sourceY) {
      for(int x=0;x<width;x++)line[x]=palette[frame->line[sourceY][fullscreen?x*256/width:x]];
      previousSource=sourceY;
    }
    tft.pushImage(left,top+y,width,1,line);
  }
  tft.endWrite();
  cacheValid=frameCache!=nullptr;
}
static viddriver_t video={"Freslest ILI9488",videoInit,videoShutdown,setMode,setPalette,clearVideo,lockWrite,freeWrite,blit,false};
extern "C" void osd_getvideoinfo(vidinfo_t* info) {info->default_width=256;info->default_height=240;info->driver=&video;}
extern "C" void osd_getsoundinfo(sndinfo_t* info) {info->sample_rate=FRESLEST_NES_SAMPLE_HZ;info->bps=16;}
extern "C" void osd_setsound(void(*)(void*,int)) {}
extern "C" void osd_submit_audio(const short* pcm,int count) {if(count>0)nesAudioSubmit(pcm,(size_t)count);}
extern "C" void osd_getinput() {}
extern "C" void osd_getmouse(int* x,int* y,int* buttons) {*x=*y=*buttons=0;}
extern "C" int osd_init() {return 0;}
extern "C" void osd_shutdown() {}
extern "C" int osd_main(int,char*[]) {return -1;}
extern "C" int osd_installtimer(int,void*,int,void*,int) {return 0;}
extern "C" void osd_fullname(char* out,const char* in) {strlcpy(out,in,PATH_MAX);}
extern "C" char* osd_newextension(char* s,char* ext) {
  char* dot=strrchr(s,'.');char* slash=strrchr(s,'/');
  if(!dot||(slash&&dot<slash))dot=s+strlen(s);
  // All core callers use PATH_MAX-sized arrays.
  strlcpy(dot,ext,PATH_MAX-(dot-s));return s;
}
extern "C" int osd_makesnapname(char*,int) {return -1;}
bool nesCoreMapperSupported(unsigned mapper) {return mapper<=255&&mmc_peek(mapper);}
void nesCoreStop() {
  nesAudioStop();
  if(machine) {nes_destroy(&machine);machine=nullptr;}
  vid_shutdown();gui_shutdown();oldPad=0;
}
bool nesCoreStart(const char* filename) {
  FILE* file=fopen(filename,"rb");if(!file)return false;
  uint8_t header[16];size_t length=fread(header,1,16,file);fseek(file,0,SEEK_END);long size=ftell(file);fclose(file);
  INesInfo parsed;if(size<0||!parseINes(header,length,(size_t)size,parsed)||!nesCoreMapperSupported(parsed.mapper))return false;
  if(machine)nesCoreStop();
  event_init();event_set_system(system_nes);gui_init();
  if(vid_init(256,240,&video)!=0)return false;
  machine=nes_create();
  if(!machine) {vid_shutdown();return false;}
  if(nes_insertcart(filename,machine)!=0) {machine=nullptr;vid_shutdown();return false;}
  if(vid_setmode(256,240)!=0) {nesCoreStop();return false;}
  freslest_nes_prepare();oldPad=0;
  nesAudioSetVolume(soundEnabled,(unsigned)soundVolume);
  nesAudioStart();return true;
}
void nesCoreFrame(bool draw,uint8_t pad) {
  if(!machine)return;
  const int events[8]={event_joypad1_a,event_joypad1_b,event_joypad1_select,event_joypad1_start,event_joypad1_up,event_joypad1_down,event_joypad1_left,event_joypad1_right};
  for(int i=0;i<8;i++)if((pad^oldPad)&(1<<i)) {event_t f=event_get(events[i]);if(f)f((pad&(1<<i))?INP_STATE_MAKE:INP_STATE_BREAK);}
  oldPad=pad;freslest_nes_step(draw);
}
