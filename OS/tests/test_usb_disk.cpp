#include "nes_usb_disk.h"
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace NesUsbDisk;
static void put16(uint8_t* p,unsigned n){p[0]=n;p[1]=n>>8;}
static void put32(uint8_t* p,unsigned n){put16(p,n);put16(p+2,n>>16);}
static void fat(std::vector<uint8_t>& disk,unsigned cluster,unsigned next){for(unsigned table=0;table<2;table++)put16(disk.data()+(1+table*32)*512+cluster*2,next);}
static unsigned formatYields=0;
static void formatYield(){++formatYields;}
int main(){
 for(size_t capacity: {DISK_SIZE,size_t(2*1024*1024),size_t(1024*1024)}) {
  std::vector<uint8_t> staged(capacity,0xa5);formatYields=0;
  assert(format(staged.data(),capacity,formatYield));assert(formatYields==capacity/65536);
  Volume formatted;assert(formatted.mount(staged.data(),capacity));assert(staged.back()==0);
 }
 std::vector<uint8_t> disk(DISK_SIZE);assert(format(disk.data(),disk.size()));Volume volume;assert(volume.mount(disk.data(),disk.size()));unsigned index=0;RomFile rom;assert(!volume.nextRom(index,rom));
 uint8_t* entry=disk.data()+65*512+64;memcpy(entry,"MARIO   NES",11);entry[11]=0x20;put16(entry+26,3);put32(entry+28,700);
 fat(disk,3,5);fat(disk,5,0xffff);memset(disk.data()+82*512,0x35,512);memset(disk.data()+84*512,0x64,512);
 index=0;assert(volume.nextRom(index,rom)&&!strcmp(rom.name,"MARIO.NES")&&rom.size==700);assert(volume.validChain(rom));
 uint8_t buffer[700];assert(volume.read(rom,0,buffer,700));for(unsigned i=0;i<700;i++)assert(buffer[i]==(i<512?0x35:0x64));
 assert(volume.read(rom,500,buffer,200));assert(buffer[0]==0x35&&buffer[12]==0x64);assert(!volume.read(rom,699,buffer,2));assert(!volume.read(rom,701,buffer,0));
 fat(disk,3,3);assert(!volume.validChain(rom));fat(disk,3,0);assert(!volume.validChain(rom));fat(disk,3,0xfff7);assert(!volume.validChain(rom));fat(disk,3,5);
 RomFile bad=rom;bad.cluster=1;assert(!volume.validChain(bad));bad=rom;bad.size=DISK_SIZE+1;assert(!volume.validChain(bad));
 unsigned char saved=disk[13];disk[13]=3;assert(!volume.mount(disk.data(),disk.size()));disk[13]=saved;assert(volume.mount(disk.data(),disk.size()));
 uint8_t* longEntry=entry;uint8_t* shortEntry=entry+32;memcpy(shortEntry,entry,32);memset(longEntry,0xff,32);longEntry[0]=0x41;longEntry[11]=0x0f;longEntry[12]=0;put16(longEntry+26,0);
 unsigned char sum=0;for(int i=0;i<11;i++)sum=((sum&1)?0x80:0)+(sum>>1)+shortEntry[i];longEntry[13]=sum;
 const char* longName="My Game.nes";const int positions[]={1,3,5,7,9,14,16,18,20,22,24,28,30};
 for(unsigned i=0;i<13;i++)put16(longEntry+positions[i],i<strlen(longName)?longName[i]:i==strlen(longName)?0:0xffff);
 index=0;assert(volume.nextRom(index,rom)&&!strcmp(rom.name,longName));
 put16(longEntry+positions[2],'/');index=0;assert(volume.nextRom(index,rom)&&!strcmp(rom.name,"MARIO.NES"));
 for(unsigned i=0;i<DISK_SIZE;i+=7919){uint8_t old=disk[i];disk[i]^=0xff;Volume probe;probe.mount(disk.data(),disk.size());disk[i]=old;}
 std::vector<uint8_t> small(512*1024);auto b=small.data();put16(b+11,512);b[13]=1;put16(b+14,1);b[16]=2;put16(b+17,32);put16(b+19,1024);put16(b+22,3);b[510]=0x55;b[511]=0xaa;
 auto fat12=[&](unsigned cluster,unsigned next){for(unsigned table=0;table<2;table++){auto p=b+(1+table*3)*512+cluster+cluster/2;if(cluster&1){p[0]=(p[0]&15)|((next&15)<<4);p[1]=next>>4;}else{p[0]=next;p[1]=(p[1]&0xf0)|(next>>8);}}};
 fat12(3,6);fat12(6,0xfff);entry=b+7*512;memcpy(entry,"SMALL   NES",11);entry[11]=0x20;put16(entry+26,3);put32(entry+28,700);
 memset(b+10*512,0x17,512);memset(b+13*512,0x28,512);assert(volume.mount(b,small.size()));index=0;assert(volume.nextRom(index,rom)&&volume.validChain(rom));assert(volume.read(rom,0,buffer,700));assert(buffer[0]==0x17&&buffer[511]==0x17&&buffer[512]==0x28&&buffer[699]==0x28);fat12(3,0xff7);assert(!volume.validChain(rom));
 puts("PASS: USB FAT16 format, FAT12/FAT16 fragmented reads, Windows short/long names, corrupt/cyclic/truncated chains, geometry and bounds");
}
