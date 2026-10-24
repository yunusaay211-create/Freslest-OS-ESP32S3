#include "nes_usb.h"
#include "nes_usb_disk.h"
#include "storage_init.h"
#include "ines_header.h"
#ifdef FRESLEST_USB_HOST_TEST
#include "usb_host_platform.h"
#else
#include "apps.h"
#include "nes_library.h"
#include "nes_core.h"
#include "audio_manager.h"
#include "settings.h"
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include <freslest_ota.h>
#include <LittleFS.h>
#include <USB.h>
#include <USBMSC.h>
#include <tusb.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif
#include <atomic>
#include <cstring>

namespace {
USBMSC& storage(){static USBMSC device;return device;}
SemaphoreHandle_t diskMutex=nullptr;
uint8_t* disk=nullptr;
size_t diskBytes=0;
bool accepting=false,usbStarted=false,fullScreen=true;
std::atomic<bool> ejectRequested{false};
std::atomic<uint32_t> bytesWritten{0};
enum State {INACTIVE,TRANSFERRING,QUIESCING,IMPORTING,DONE,FAILED};
State state=INACTIVE;
uint32_t lastDraw=0,importAt=0;
size_t freeGameBytes=0;
unsigned imported=0,unchanged=0,rejected=0;
char detail[96]={};

void cacheFreeSpace() {
  size_t total=LittleFS.totalBytes(),used=LittleFS.usedBytes();
  freeGameBytes=total>used?total-used:0;
}
void cooperate() {delay(1);}

int32_t readDisk(uint32_t lba,uint32_t offset,void* output,uint32_t length) {
  if(!diskMutex||!output)return -1;
  uint64_t address=(uint64_t)lba*512+offset;
  xSemaphoreTake(diskMutex,portMAX_DELAY);
  bool valid=disk&&address+length<=diskBytes;if(valid)memcpy(output,disk+(size_t)address,length);
  xSemaphoreGive(diskMutex);return valid?(int32_t)length:-1;
}
int32_t writeDisk(uint32_t lba,uint32_t offset,uint8_t* input,uint32_t length) {
  if(!diskMutex||!input)return -1;
  uint64_t address=(uint64_t)lba*512+offset;
  xSemaphoreTake(diskMutex,portMAX_DELAY);
  bool valid=disk&&accepting&&address+length<=diskBytes;
  if(valid) {memcpy(disk+(size_t)address,input,length);bytesWritten.fetch_add(length);}
  xSemaphoreGive(diskMutex);return valid?(int32_t)length:-1;
}
bool startStop(uint8_t,bool start,bool eject) {
  if(!start&&eject&&diskMutex) {
    xSemaphoreTake(diskMutex,portMAX_DELAY);accepting=false;xSemaphoreGive(diskMutex);
    storage().mediaPresent(false);ejectRequested.store(true);
  }
  return true;
}
void drawWrites() {
  if(!fullScreen||state!=TRANSFERRING)return;
  tft.fillRect(22,173,436,16,TH().bg);
  tft.setTextSize(1);tft.setTextColor(TH().text,TH().bg);tft.setCursor(22,178);
  tft.printf("Transfer writes: %lu KB",(unsigned long)(bytesWritten.load()/1024));
}
void draw() {
  if(!fullScreen)return;
  tft.fillScreen(TH().bg);drawHeader("NES USB Transfer");clearBody();
  cooperate();
  drawFooter(state==TRANSFERRING?"PC Eject / OK Import":"OK Return",state==TRANSFERRING?"BACK Cancel":"BACK Library");
  tft.setTextColor(TH().text,TH().bg);tft.setTextSize(2);tft.setCursor(22,62);
  tft.print(state==TRANSFERRING?"FRESLEST USB DRIVE":state==QUIESCING?"FINISHING USB TRANSFER":state==IMPORTING?"IMPORTING ROMS":state==DONE?"TRANSFER COMPLETE":"USB TRANSFER ERROR");
  tft.setTextSize(1);tft.setCursor(22,106);
  if(state==TRANSFERRING) {
    tft.print("Connect the native USB port (D-19 / D+20).");
    tft.setCursor(22,130);tft.print("Copy .nes files to the FRESLEST drive root.");
    tft.setCursor(22,154);tft.print("EJECT on PC. If import does not start, press OK.");
    tft.setCursor(22,178);tft.printf("Transfer writes: %lu KB",(unsigned long)(bytesWritten.load()/1024));
    tft.setCursor(22,202);tft.printf("Free game flash: %u KB (320 KB total)",(unsigned)(freeGameBytes/1024));
    tft.setCursor(22,226);tft.print("The USB drive is temporary staging space.");
    tft.setCursor(22,250);tft.print("Installed ROMs and Note stay in internal flash.");
  } else {
    tft.printf("Imported: %u   Unchanged: %u   Rejected: %u",imported,unchanged,rejected);
    char first[71];snprintf(first,sizeof(first),"%.70s",detail);
    tft.setCursor(22,142);tft.print(first);
    if(strlen(detail)>70) {tft.setCursor(22,158);tft.print(detail+70);}
    tft.setCursor(22,194);tft.print(state==IMPORTING||state==QUIESCING?"Keep the device powered.":"Press OK to return to the NES Library.");
    if(state==DONE||state==FAILED) {tft.setCursor(22,226);tft.print("FIRE: reconnect with a fresh transfer disk.");}
  }
  cooperate();
}
bool sameFile(const char* path,const NesUsbDisk::Volume& volume,const NesUsbDisk::RomFile& rom) {
  File file=LittleFS.open(path,"r");if(!file||file.size()!=rom.size)return false;
  uint8_t a[512],b[512];
  for(uint32_t offset=0;offset<rom.size;) {
    size_t n=rom.size-offset;if(n>sizeof(a))n=sizeof(a);
    if(file.read(a,n)!=n||!volume.read(rom,offset,b,n)||memcmp(a,b,n))return false;
    offset+=n;
    if((offset&8191)==0)cooperate();
  }
  return true;
}
unsigned installedCount() {
  unsigned count=0;File dir=LittleFS.open("/games/nes");if(!dir)return 0;
  for(File f=dir.openNextFile();f;f=dir.openNextFile()) {
    String name=f.name();name.toLowerCase();if(!f.isDirectory()&&name.endsWith(".nes"))++count;f.close();
  }
  return count;
}
void importRoms() {
  state=IMPORTING;draw();
  NesUsbDisk::Volume volume;
  if(!volume.mount(disk,diskBytes)) {state=FAILED;snprintf(detail,sizeof(detail),"Invalid USB filesystem. Re-enter USB Transfer.");return;}
  if(!freslestStorageEnsure()) {state=FAILED;snprintf(detail,sizeof(detail),"%s",freslestStorageStatus());return;}
  if(!LittleFS.exists("/games"))LittleFS.mkdir("/games");
  if(!LittleFS.exists("/games/nes"))LittleFS.mkdir("/games/nes");
  unsigned count=installedCount(),index=0;NesUsbDisk::RomFile rom;
  static constexpr const char* TEMP="/games/nes/.usb.tmp";
  LittleFS.remove(TEMP);
  while(volume.nextRom(index,rom)) {
    char path[64];snprintf(path,sizeof(path),"/games/nes/%s",rom.name);
    uint8_t header[16];INesInfo info;
    if(!volume.validChain(rom)||!volume.read(rom,0,header,sizeof(header))) {
      ++rejected;snprintf(detail,sizeof(detail),"Broken/incomplete USB file: %.48s",rom.name);Serial.printf("NES USB: %s\n",detail);cooperate();continue;
    }
    if(!parseINes(header,16,rom.size,info)) {
      ++rejected;snprintf(detail,sizeof(detail),"%.42s: %.48s",info.error,rom.name);Serial.printf("NES USB: %s\n",detail);cooperate();continue;
    }
    if(!nesCoreMapperSupported(info.mapper)) {
      ++rejected;snprintf(detail,sizeof(detail),"Unsupported mapper %u: %.48s",info.mapper,rom.name);Serial.printf("NES USB: %s\n",detail);cooperate();continue;
    }
    bool exists=LittleFS.exists(path);
    if(exists&&sameFile(path,volume,rom)) {++unchanged;continue;}
    cacheFreeSpace();size_t free=freeGameBytes;
    if((!exists&&count>=32)||free<8192||rom.size>free-8192) {
      ++rejected;snprintf(detail,sizeof(detail),"Storage full: need %lu KB, free %u KB. Old files kept.",(unsigned long)((rom.size+1023)/1024),(unsigned)(free/1024));Serial.printf("NES USB: %s\n",detail);cooperate();continue;
    }
    File out=LittleFS.open(TEMP,"w");bool ok=(bool)out;uint8_t buffer[2048];
    for(uint32_t offset=0;ok&&offset<rom.size;) {
      size_t n=rom.size-offset;if(n>sizeof(buffer))n=sizeof(buffer);
      ok=volume.read(rom,offset,buffer,n)&&out.write(buffer,n)==n;offset+=n;
      audioUpdate(millis());delay(1);
    }
    out.flush();out.close();
    if(ok)ok=LittleFS.rename(TEMP,path); // LittleFS rename atomically replaces a same-name ROM.
    if(!ok) {LittleFS.remove(TEMP);++rejected;snprintf(detail,sizeof(detail),"Write failed: %.48s (old file kept)",rom.name);}
    else {++imported;if(!exists)++count;if(!rejected)snprintf(detail,sizeof(detail),"Saved: %.48s",rom.name);}
    Serial.printf("NES USB: %s\n",detail);draw();
  }
  if(!imported&&!rejected&&!unchanged)snprintf(detail,sizeof(detail),"No .nes files found in the USB drive root.");
  cacheFreeSpace();
  state=DONE;
}
}
void nesUsbExit() {
  if(usbStarted){storage().mediaPresent(false);tud_disconnect();}
  uint8_t* old=nullptr;
  if(diskMutex) {xSemaphoreTake(diskMutex,portMAX_DELAY);accepting=false;old=disk;disk=nullptr;diskBytes=0;xSemaphoreGive(diskMutex);}
  if(old)heap_caps_free(old);
  ejectRequested.store(false);state=INACTIVE;
}
void nesUsbEnter(bool showScreen) {
#ifndef FRESLEST_USB_HOST_TEST
  if(!usbInputDeviceMode()){
    fullScreen=true;state=FAILED;
    if(!freslest_ota_ready()){snprintf(detail,sizeof(detail),"Startup check: wait 10 seconds. FIRE retries.");draw();return;}
    if(!usbInputSwitchMode(true)){snprintf(detail,sizeof(detail),"Cannot save USB mode. Retry.");draw();}
    return;
  }
#endif
  fullScreen=showScreen;
  // Details are a view of the existing session, including its final error.
  // Only an explicit reconnect (FIRE inside details) starts a fresh disk.
  if(state!=INACTIVE) {draw();return;}
  nesUsbExit();nesPlayerExit();audioStop();imported=unchanged=rejected=0;detail[0]=0;bytesWritten.store(0);
  // Drain the disconnected session before publishing a new disk to callbacks.
  if(usbStarted)delay(300);
  cacheFreeSpace();
  if(!diskMutex)diskMutex=xSemaphoreCreateMutex();
  if(!diskMutex) {state=FAILED;snprintf(detail,sizeof(detail),"E202: USB disk mutex unavailable");draw();return;}
  uint8_t* memory=nullptr;size_t capacity=NesUsbDisk::DISK_SIZE;
  for(;capacity>=1024*1024;capacity/=2) {
    memory=(uint8_t*)heap_caps_malloc(capacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(memory)break;
  }
  if(!memory||!NesUsbDisk::format(memory,capacity,cooperate)) {
    if(memory)heap_caps_free(memory);
    state=FAILED;
    snprintf(detail,sizeof(detail),"E203: Cannot allocate a 1-4 MB USB disk");draw();return;
  }
  xSemaphoreTake(diskMutex,portMAX_DELAY);disk=memory;diskBytes=capacity;ejectRequested.store(false);accepting=true;xSemaphoreGive(diskMutex);
  storage().vendorID("FRESLEST");storage().productID("NES TRANSFER");storage().productRevision("1.0");
  storage().onRead(readDisk);storage().onWrite(writeDisk);storage().onStartStop(startStop);storage().isWritable(true);storage().mediaPresent(true);
  bool ok=storage().begin(diskBytes/512,512);
  if(ok&&!usbStarted) {
    // A pure MSC device uses per-interface class descriptors, rather than the
    // Arduino default IAD/composite class. UART logging stays on the other port.
    USB.usbClass(0);USB.usbSubClass(0);USB.usbProtocol(0);
    USB.productName("Freslest NES Transfer");USB.manufacturerName("Freslest");
    USB.firmwareVersion(0x0601);usbStarted=USB.begin();ok=usbStarted;
  } else if(ok) {ok=tud_connect();}
  if(!ok) {nesUsbExit();state=FAILED;snprintf(detail,sizeof(detail),"E205: USB init failed. Use native USB/OTG.");}
  else state=TRANSFERRING;
  lastDraw=millis();draw();
  Serial.printf("NES USB: %s. Native USB D-19 / D+20; programming port stays serial.\n",ok?"MSC enabled":"initialization failed");
}
bool nesUsbPoll() {
  if(state==TRANSFERRING&&ejectRequested.exchange(false)) {
    // Let the USB task acknowledge the host's eject before disconnecting.
    // Writes are already blocked by startStop (or the explicit OK path).
    state=QUIESCING;storage().mediaPresent(false);importAt=millis()+200;draw();
  }
  if(state==QUIESCING&&(int32_t)(millis()-importAt)>=0) {
    tud_disconnect();cooperate();importRoms();draw();return true;
  }
  return false;
}
const char* nesUsbStatus() {
  if(state==FAILED)return "USB ERROR - FIRE for details / retry";
  if(state==DONE) {
    static char status[96];
    snprintf(status,sizeof(status),"USB: %u imported, %u unchanged, %u rejected - FIRE details",imported,unchanged,rejected);
    return status;
  }
  if(state==QUIESCING||state==IMPORTING)return "USB importing - keep power connected";
  if(state==TRANSFERRING)return tud_mounted()?"USB connected - copy NES, then EJECT on PC":"USB ready - connect USB/OTG port to PC";
  return "USB inactive";
}
void nesUsbLoop(uint32_t now) {
  nesUsbPoll();
  if((state==DONE||state==FAILED)&&bFire.pressed()) {nesUsbExit();nesUsbEnter(true);return;}
  if(state==TRANSFERRING&&bOk.pressed()) {startStop(0,false,true);nesUsbPoll();return;}
  if((state==DONE||state==FAILED)&&bOk.pressed()) {
#ifndef FRESLEST_USB_HOST_TEST
    if(usbInputSwitchMode(false))enterApp(APP_NES_LIBRARY);
#else
    enterApp(APP_NES_LIBRARY);
#endif
    return;
  }
  if(bBack.pressed()&&state!=QUIESCING&&state!=IMPORTING) {if(state==TRANSFERRING)nesUsbExit();
#ifndef FRESLEST_USB_HOST_TEST
    if(usbInputSwitchMode(false))enterApp(APP_NES_LIBRARY);
#else
    enterApp(APP_NES_LIBRARY);
#endif
    return;
}
  if(state==TRANSFERRING&&now-lastDraw>=1000) {lastDraw=now;drawWrites();}
}
