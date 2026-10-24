#include "nes_library.h"
#include "apps.h"
#include "../../gfx.h"
#include "../../input.h"
#include "audio_manager.h"
#include "nes_usb.h"
#include "../../usb_input.h"

#include <LittleFS.h>
#include <FS.h>
#include "nes_core.h"
#include "ines_header.h"

namespace {
constexpr const char* NES_DIR = "/games/nes";
constexpr int MAX_NES_ROMS = 32;
constexpr int ROW_H = 38;
constexpr int LIST_Y = 56;
constexpr int VISIBLE_ROWS = 4;

NesRomInfo roms[MAX_NES_ROMS];
int romCount = 0;
int selected = 0;
int topIndex = 0;
bool scanDone = false;

String cleanRomName(const String& path) {
  String s = path;
  int slash = s.lastIndexOf('/');
  if (slash >= 0) s = s.substring(slash + 1);
  String lower=s;lower.toLowerCase();
  if(lower.endsWith(".nes"))s.remove(s.length()-4);
  s.replace('_', ' ');
  return s;
}

bool isNesFileName(const String& name) {
  String s = name;
  s.toLowerCase();
  return s.endsWith(".nes");
}

bool readINesHeader(const String& path, NesRomInfo& out) {
  File f=LittleFS.open(path,"r");
  if(!f) {out.error="Cannot open ROM";return false;}
  uint8_t h[16];size_t size=f.size(),n=f.read(h,16);f.close();INesInfo info;
  if(!parseINes(h,n,size,info)) {out.error=info.error;return false;}
  out.prgKB=info.prgKB;out.chrKB=info.chrKB;out.mapper=info.mapper;
  if(!nesCoreMapperSupported(out.mapper)) {out.error="Unsupported mapper";return false;}
  out.valid=true;out.error="";return true;
}

void sortRoms() {
  for (int i = 0; i < romCount - 1; ++i) {
    for (int j = i + 1; j < romCount; ++j) {
      String a = roms[i].name; a.toLowerCase();
      String b = roms[j].name; b.toLowerCase();
      if (b.compareTo(a) < 0) {
        NesRomInfo tmp = roms[i];
        roms[i] = roms[j];
        roms[j] = tmp;
      }
    }
  }
}

void ensureSelectionVisible() {
  if (selected < topIndex) topIndex = selected;
  if (selected >= topIndex + VISIBLE_ROWS) topIndex = selected - VISIBLE_ROWS + 1;
  if (topIndex < 0) topIndex = 0;
}

void drawLibraryFull(){
 tft.fillScreen(TH().bg);drawHeader("NES Library");clearBody();drawFooter("U/D Select OK Open","FIRE USB BACK Game");
 tft.setTextSize(1);tft.setTextColor(TH().dim,TH().bg);tft.setCursor(20,46);tft.printf("%d games | L/R Page | USB keyboard supported",romCount);
 if(!romCount){
  tft.setTextSize(2);tft.setCursor(30,92);tft.print("No NES games found");tft.setTextSize(1);tft.setCursor(30,130);tft.print("Choose USB Transfer below to add .nes files.");tft.setCursor(30,154);tft.print("Copy to drive root, EJECT, then return here.");
 }
 for(int row=0;row<VISIBLE_ROWS&&topIndex+row<romCount;++row){
  int i=topIndex+row,y=LIST_Y+row*ROW_H;bool sel=i==selected;uint16_t bg=sel?TH().selbg:TH().bg;
  tft.fillRoundRect(20,y+2,440,32,8,bg);if(sel)tft.drawRoundRect(20,y+2,440,32,8,TH().accent);
  tft.setTextSize(2);tft.setTextColor(sel?TH().text:TH().dim,bg);tft.setCursor(30,y+10);
  String shown=roms[i].name;if(shown.length()>31)shown=shown.substring(0,28)+"...";tft.print(sel?"> ":"  ");tft.print(shown);
 }
 if(selected<romCount){tft.setTextSize(1);tft.setTextColor(roms[selected].valid?TH().dim:TFT_RED,TH().bg);tft.setCursor(24,218);
  if(roms[selected].valid)tft.printf("Mapper %u | PRG %u KB | CHR %u KB",roms[selected].mapper,roms[selected].prgKB,roms[selected].chrKB);else tft.print(roms[selected].error);
 }
 const char* actions[]={"USB Transfer to PC","Fullscreen"};
 for(int i=0;i<2;i++){int y=234+i*31;bool sel=selected==romCount+i;uint16_t bg=sel?TH().selbg:TH().panel;
  tft.fillRoundRect(20,y,440,28,6,bg);if(sel)tft.drawRoundRect(20,y,440,28,6,TH().accent);tft.setTextSize(2);tft.setTextColor(TH().text,bg);tft.setCursor(32,y+6);tft.print(actions[i]);if(i==1)tft.print(nesFullscreen?": ON":": OFF");
 }
}


}

void nesLibraryRefresh() {
  romCount = 0;
  selected = 0;
  topIndex = 0;
  scanDone = true;

  if (!LittleFS.exists("/games")) LittleFS.mkdir("/games");
  if (!LittleFS.exists(NES_DIR)) LittleFS.mkdir(NES_DIR);
  File dir = LittleFS.open(NES_DIR);
  if (!dir || !dir.isDirectory()) return;

  File f = dir.openNextFile();
  while (f && romCount < MAX_NES_ROMS) {
    if (!f.isDirectory()) {
      String path = f.path();
      if (path.length() == 0) path = f.name();
      if(!path.startsWith("/"))path=String(NES_DIR)+"/"+path;
      if(path.startsWith("/littlefs/"))path=path.substring(9);
      if (isNesFileName(path)) {
        NesRomInfo info;
        info.path = path;
        info.name = cleanRomName(path);
        f.close();
        readINesHeader(path, info);
        roms[romCount++] = info;
        f = dir.openNextFile();
        continue;
      }
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  sortRoms();
}

void nesLibraryEnter() {
  nesLibraryRefresh();
  drawLibraryFull();
}

void nesLibraryLoop(uint32_t now){
 (void)now;if(bFire.pressed()){enterApp(APP_USB_TRANSFER);return;}
 int count=romCount+2;bool changed=false,open=bOk.pressed();
 if(bUp.pressed()){selected=(selected+count-1)%count;changed=true;}
 if(bDown.pressed()){selected=(selected+1)%count;changed=true;}
 if(bLeft.pressed()||bRight.pressed()){selected=constrain(selected+(bRight.pressed()?VISIBLE_ROWS:-VISIBLE_ROWS),0,count-1);changed=true;}
 if(usbInputHit(20,LIST_Y,440,VISIBLE_ROWS*ROW_H)){int i=topIndex+(usbPointer.y-LIST_Y)/ROW_H;if(i<romCount){selected=i;open=true;changed=true;}}
 if(usbInputHit(20,234,440,28)){selected=romCount;open=true;}
 if(usbInputHit(20,265,440,28)){selected=romCount+1;open=true;}
 if(changed){if(selected<romCount)ensureSelectionVisible();drawLibraryFull();}
 if(open){
  if(selected==romCount){enterApp(APP_USB_TRANSFER);return;}
  if(selected==romCount+1){nesFullscreen=!nesFullscreen;settingsSave();drawLibraryFull();return;}
  if(roms[selected].valid)enterApp(APP_NES_PLAYER);
 }
}

const NesRomInfo* nesSelectedRom() {
  if (romCount <= 0 || selected < 0 || selected >= romCount) return nullptr;
  return &roms[selected];
}

namespace {
bool playing=false;
uint32_t lastNesUs=0;
void drawControlsHelp() {
  tft.setTextSize(1);tft.setTextColor(TH().dim,TH().bg);
  tft.setCursor(6,74);tft.print("UP+OK");tft.setCursor(6,90);tft.print("START");
  tft.setCursor(6,118);tft.print("DOWN+OK");tft.setCursor(6,134);tft.print("SELECT");
}

}
bool nesPlayerFullscreen() {return playing&&nesFullscreen;}
void nesPlayerExit() {
  if(!playing)return;
  nesCoreStop();playing=false;
}
void nesPlayerEnter() {
  nesPlayerExit();
  tft.fillScreen(0);
  if(!nesFullscreen) {drawHeader("NES Player");drawFooter("OK=A FIRE=B","BACK Library");clearBody();}
  nesCoreSetFullscreen(nesFullscreen);
  const NesRomInfo* rom=nesSelectedRom();
  String filename=rom?String("/littlefs")+rom->path:String("");
  if(!rom||!rom->valid||!nesCoreStart(filename.c_str())) {
    drawHeader("NES Player");drawFooter("OK Retry","BACK Library");clearBody();
    tft.setTextSize(2);tft.setTextColor(TFT_RED,TH().bg);tft.setCursor(30,120);tft.print("ROM failed to start");
    tft.setTextSize(1);tft.setCursor(30,158);tft.print("Check LittleFS, PSRAM and the ROM file.");return;
  }
  inputNesBegin();audioGame();playing=true;lastNesUs=micros();if(!nesFullscreen)drawControlsHelp();
}
void nesPlayerLoop(uint32_t now) {
  if(!playing) {if(bOk.pressed())nesPlayerEnter();return;}
  uint32_t us=micros(),elapsed=us-lastNesUs;
  unsigned frames=elapsed/16667;
  if(!frames)return;
  if(frames>3) {frames=3;lastNesUs=us;}else lastNesUs+=frames*16667;
  for(unsigned i=0;i<frames;i++)nesCoreFrame(i+1==frames,inputNesPad());
  (void)now;
}
