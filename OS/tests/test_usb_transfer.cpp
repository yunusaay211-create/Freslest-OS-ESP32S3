#define FRESLEST_USB_HOST_TEST 1
#include "../freslest/src/freslest/nes_usb.cpp"
#include <cassert>
#include <fstream>
#include <unistd.h>
static std::vector<uint8_t> romData(uint8_t value) {
 std::vector<uint8_t> rom(16+16384+8192,value);memset(rom.data(),0,16);memcpy(rom.data(),"NES\x1a",4);rom[4]=rom[5]=1;return rom;
}
static void put16(uint8_t* p,unsigned value){p[0]=value;p[1]=value>>8;}
static void put32(uint8_t* p,unsigned value){put16(p,value);put16(p+2,value>>16);}
static void copyFromHost(const std::vector<uint8_t>& rom,bool cyclic=false){
 std::vector<uint8_t> image(disk,disk+NesUsbDisk::DISK_SIZE);
 uint8_t* entry=image.data()+65*512+64;memcpy(entry,"GAME    NES",11);entry[11]=0x20;put16(entry+26,3);put32(entry+28,rom.size());
 unsigned clusters=(rom.size()+511)/512;
 for(unsigned i=0;i<clusters;i++){
  unsigned cluster=3+i,next=cyclic?3:i+1<clusters?cluster+1:0xffff;
  for(unsigned table=0;table<2;table++)put16(image.data()+(1+table*32)*512+cluster*2,next);
  size_t offset=i*512,n=std::min<size_t>(512,rom.size()-offset);memcpy(image.data()+(82+i)*512,rom.data()+offset,n);
 }
 assert(writeDisk(0,0,image.data(),image.size())==(int32_t)image.size());
}
static std::vector<uint8_t> installed(){
 std::ifstream file(testRoot/"games/nes/GAME.NES",std::ios::binary);return {std::istreambuf_iterator<char>(file),{}};
}
static void ejectAndImport(){assert(startStop(0,false,true));assert(!accepting&&!storage().media);nesUsbLoop(millis());assert(state==QUIESCING);delay(200);nesUsbLoop(millis());assert(state==DONE);}
static void begin(){nesUsbEnter();assert(state==TRANSFERRING&&storage().media&&testAllocations==1&&testUsbConnected);}
int main(){
 testRoot=std::filesystem::temp_directory_path()/("freslest_usb_"+std::to_string(getpid()));std::filesystem::create_directories(testRoot);
 std::ofstream(testRoot/"note.txt")<<"Keep my note";
 auto original=romData(0x33),replacement=romData(0x44);
 // Library auto-starts MSC. FIRE only opens details and keeps the staged data.
 nesUsbEnter(false);assert(state==TRANSFERRING&&storage().media&&testAllocations==1&&testUsbConnected&&!fullScreen);
 uint8_t marker=0x69;assert(writeDisk(8000,0,&marker,1)==1);
 nesUsbEnter(true);assert(fullScreen&&disk[8000*512]==marker&&testAllocations==1);
 // Details and progress updates must not query flash or reformat the disk.
 unsigned redraws=testFullRedraws,queries=testFsQueries;
 for(int i=0;i<15;++i){delay(1000);nesUsbLoop(millis());}
 assert(testFullRedraws==redraws&&testFsQueries==queries&&disk[8000*512]==marker);
 nesUsbEnter(false);assert(!fullScreen&&disk[8000*512]==marker&&testAllocations==1);
 nesUsbExit();assert(!testAllocations);
 begin();uint8_t sector[512];assert(readDisk(0,0,sector,sizeof(sector))==512&&sector[510]==0x55);
 assert(readDisk(UINT32_MAX,UINT32_MAX,sector,512)==-1&&writeDisk(UINT32_MAX,0,sector,512)==-1);
 copyFromHost(original);ejectAndImport();assert(imported==1&&!rejected&&installed()==original);assert(writeDisk(0,0,sector,512)==-1);
 uint8_t* completedDisk=disk;std::string completedDetail=detail;
 for(int i=0;i<20;++i){nesUsbEnter(false);nesUsbEnter(true);assert(state==DONE&&disk==completedDisk&&detail==completedDetail&&imported==1&&testAllocations==1);}
 assert(strstr(nesUsbStatus(),"1 imported"));
 bBack.edge=true;nesUsbLoop(millis());bBack.edge=false;assert(state==DONE&&imported==1);
 // Only FIRE inside the result view explicitly reconnects with a fresh disk.
 bFire.edge=true;nesUsbLoop(millis());bFire.edge=false;assert(state==TRANSFERRING&&!imported&&testAllocations==1);
 nesUsbExit();assert(!testAllocations);
 begin();copyFromHost(original);ejectAndImport();assert(unchanged==1&&!imported&&!rejected);nesUsbExit();
 begin();copyFromHost(replacement);testWriteFail=true;ejectAndImport();assert(rejected==1&&installed()==original&&!LittleFS.exists("/games/nes/.usb.tmp"));testWriteFail=false;nesUsbExit();
 begin();copyFromHost(replacement);testRenameFail=true;ejectAndImport();assert(rejected==1&&installed()==original);testRenameFail=false;nesUsbExit();
 begin();copyFromHost(replacement);testFreeBytes=4096;ejectAndImport();assert(rejected==1&&installed()==original);assert(strstr(detail,"need")&&strstr(detail,"free"));nesUsbEnter(false);nesUsbEnter(true);assert(rejected==1&&strstr(detail,"Storage full"));testFreeBytes=-1;nesUsbExit();
 begin();auto invalid=replacement;invalid[7]=8;copyFromHost(invalid);ejectAndImport();assert(rejected==1&&installed()==original);nesUsbExit();
 begin();copyFromHost(replacement,true);ejectAndImport();assert(rejected==1&&installed()==original);nesUsbExit();
 begin();copyFromHost(replacement);ejectAndImport();assert(imported==1&&installed()==replacement);nesUsbExit();
 // Hosts that eject without START STOP can finish from OK. No write is
 // accepted after the request, and the 200ms acknowledgement window is kept.
 begin();copyFromHost(original);bOk.edge=true;nesUsbLoop(millis());bOk.edge=false;
 assert(state==QUIESCING&&!accepting&&installed()==replacement);
 assert(writeDisk(0,0,sector,512)==-1);delay(190);nesUsbPoll();assert(state==QUIESCING);
 delay(10);assert(nesUsbPoll()&&state==DONE&&imported==1&&installed()==original);nesUsbExit();
 replacement=original;
 begin();copyFromHost(original);bBack.edge=true;nesUsbLoop(millis());bBack.edge=false;assert(!testAllocations&&installed()==replacement&&testEnteredApp==APP_NES_LIBRARY);
 testHeapFail=true;nesUsbEnter();assert(state==FAILED&&!testAllocations);testHeapFail=false;
 nesUsbEnter(false);nesUsbEnter(true);assert(state==FAILED&&strstr(detail,"E203"));nesUsbExit();
 testMscFail=true;nesUsbEnter();assert(state==FAILED&&!testAllocations);testMscFail=false;nesUsbExit();
 for(size_t capacity: {size_t(2*1024*1024),size_t(1024*1024)}) {
  testHeapMax=capacity;begin();assert(diskBytes==capacity);NesUsbDisk::Volume volume;assert(volume.mount(disk,diskBytes));
  uint8_t byte;assert(readDisk(capacity/512,0,&byte,1)==-1);nesUsbExit();assert(!testAllocations);
 }
 testHeapMax=SIZE_MAX;testStorage=false;begin();assert(startStop(0,false,true));nesUsbPoll();delay(200);nesUsbPoll();assert(state==FAILED);nesUsbEnter(false);nesUsbEnter(true);assert(state==FAILED&&strstr(detail,"E102"));nesUsbExit();testStorage=true;
 for(int i=0;i<20;i++){begin();nesUsbExit();assert(!testAllocations);}
 std::ifstream note(testRoot/"note.txt");std::string text;std::getline(note,text);assert(text=="Keep my note");note.close();
 std::filesystem::remove_all(testRoot);delete diskMutex;diskMutex=nullptr;
 puts("PASS: actual USB callbacks, delayed eject acknowledgement, OK import fallback, preserved result/error across Library/details, explicit reconnect, partial redraws without flash queries, new/unchanged/replaced ROMs, invalid/full-storage/write-failure preservation and buffer cleanup");
}
