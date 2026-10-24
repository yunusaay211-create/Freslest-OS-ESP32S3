#include "nes_usb_disk.h"
#include <cstring>
#include <cstdio>

namespace NesUsbDisk {
static uint16_t u16(const uint8_t* p) {return (uint16_t)(p[0]|(p[1]<<8));}
static uint32_t u32(const uint8_t* p) {return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
static void put16(uint8_t* p,uint16_t n) {p[0]=n;p[1]=n>>8;}
static void put32(uint8_t* p,uint32_t n) {put16(p,n);put16(p+2,n>>16);}
bool format(uint8_t* disk,size_t size,void (*cooperate)()) {
  if(!disk||(size!=DISK_SIZE&&size!=2*1024*1024&&size!=1024*1024))return false;
  bool fat12=size!=DISK_SIZE;unsigned fatSectors=size==DISK_SIZE?32:size==2*1024*1024?12:6;
  for(size_t offset=0;offset<size;offset+=65536) {
    size_t amount=size-offset;if(amount>65536)amount=65536;
    memset(disk+offset,0,amount);if(cooperate)cooperate();
  }
  disk[0]=0xeb;disk[1]=0x3c;disk[2]=0x90;memcpy(disk+3,"FRESLEST",8);
  put16(disk+11,512);disk[13]=1;put16(disk+14,1);disk[16]=2;
  put16(disk+17,256);put16(disk+19,size/512);disk[21]=0xf8;put16(disk+22,fatSectors);
  put16(disk+24,32);put16(disk+26,64);disk[36]=0x80;disk[38]=0x29;
  put32(disk+39,0x4652534c);memcpy(disk+43,"FRESLEST   ",11);memcpy(disk+54,fat12?"FAT12   ":"FAT16   ",8);
  disk[510]=0x55;disk[511]=0xaa;
  for(unsigned i=0;i<2;i++) {
    uint8_t* fat=disk+(1+i*fatSectors)*512;if(fat12) {fat[0]=0xf8;fat[1]=fat[2]=fat[3]=0xff;fat[4]=0x0f;} else {put16(fat,0xfff8);put16(fat+2,0xffff);put16(fat+4,0xffff);}
  }
  uint8_t* root=disk+(1+2*fatSectors)*512;memcpy(root,"FRESLEST   ",11);root[11]=8;
  static const char instructions[]=
    "FRESLEST NES TRANSFER\r\n\r\n"
    "Copy .nes files into the root of this drive.\r\n"
    "Then safely EJECT FRESLEST in Windows.\r\n"
    "Wait for the device to finish importing.\r\n\r\n"
    "This is a temporary transfer disk.\r\n"
    "Installed games are stored separately in flash.\r\n"
    "Available flash space is shown on the screen.\r\n"
    "Folders and non-NES files are ignored.\r\n"
    "Keep the device powered during import.\r\n";
  root+=32;memcpy(root,"README  TXT",11);root[11]=0x20;put16(root+26,2);put32(root+28,sizeof(instructions)-1);
  memcpy(disk+(17+2*fatSectors)*512,instructions,sizeof(instructions)-1);
  return true;
}
bool Volume::mount(const uint8_t* disk,size_t size) {
  data=nullptr;
  if(!disk||size<512||disk[510]!=0x55||disk[511]!=0xaa||u16(disk+11)!=512)return false;
  uint32_t spc=disk[13],reserved=u16(disk+14),fats=disk[16],entries=u16(disk+17),fatSectors=u16(disk+22);
  uint32_t sectors=u16(disk+19);if(!sectors)sectors=u32(disk+32);
  if(!spc||spc>128||(spc&(spc-1))||!reserved||fats<1||fats>2||!entries||!fatSectors)return false;
  uint64_t first=(uint64_t)reserved+(uint64_t)fats*fatSectors+((uint64_t)entries*32+511)/512;
  if(first>=sectors||(uint64_t)sectors*512>size)return false;
  uint32_t count=(sectors-(uint32_t)first)/spc;
  if(!count||count>=65525)return false;
  bool is12=count<4085;
  uint64_t required=is12?((uint64_t)(count+2)*3+1)/2:(uint64_t)(count+2)*2;
  if(required>(uint64_t)fatSectors*512)return false;
  bytes=(size_t)sectors*512;fatOffset=reserved*512;rootOffset=(reserved+fats*fatSectors)*512;
  dataOffset=(uint32_t)first*512;clusterBytes=spc*512;clusters=count;rootEntries=entries;fat12=is12;data=disk;
  return true;
}
bool Volume::validCluster(uint16_t n) const {return n>=2&&n<clusters+2&&n<(fat12?0xff0:0xfff0);}
bool Volume::end(uint16_t n) const {return n>=(fat12?0xff8:0xfff8);}
uint16_t Volume::next(uint16_t n) const {
  if(!data||!validCluster(n))return 0;
  if(!fat12)return u16(data+fatOffset+(uint32_t)n*2);
  uint32_t offset=(uint32_t)n+n/2;uint16_t value=u16(data+fatOffset+offset);
  return n&1?value>>4:value&0xfff;
}
static uint8_t shortChecksum(const uint8_t* p) {
  uint8_t sum=0;for(int i=0;i<11;i++)sum=(uint8_t)(((sum&1)?0x80:0)+(sum>>1)+p[i]);return sum;
}
bool Volume::nextRom(unsigned& index,RomFile& file) const {
  if(!data)return false;
  char longName[256]={};unsigned expected=0;uint8_t checksum=0;bool longValid=false;
  static const unsigned positions[13]={1,3,5,7,9,14,16,18,20,22,24,28,30};
  while(index<rootEntries) {
    const uint8_t* p=data+rootOffset+(index++)*32;
    if(p[0]==0)return false;
    if(p[0]==0xe5) {longValid=false;continue;}
    if(p[11]==0x0f) {
      unsigned sequence=p[0]&0x1f;
      if(p[0]&0x40) {memset(longName,0,sizeof(longName));expected=sequence;checksum=p[13];longValid=sequence>=1&&sequence<=20;}
      if(!longValid||sequence!=expected||p[13]!=checksum||p[12]||u16(p+26)) {longValid=false;continue;}
      for(unsigned j=0;j<13;j++) {
        unsigned pos=(sequence-1)*13+j;uint16_t value=u16(p+positions[j]);
        if(pos>=sizeof(longName)-1) {if(value&&value!=0xffff)longValid=false;continue;}
        if(value==0||value==0xffff)longName[pos]=0;
        else if(value<32||value>126||value=='/'||value=='\\'||value==':')longValid=false;
        else longName[pos]=(char)value;
      }
      --expected;continue;
    }
    if(p[11]&0x18) {longValid=false;continue;}
    char shortName[13]={};unsigned n=0;
    for(unsigned j=0;j<8&&p[j]!=' ';j++)shortName[n++]=(char)p[j];
    if(p[8]!=' ') {shortName[n++]='.';for(unsigned j=8;j<11&&p[j]!=' ';j++)shortName[n++]=(char)p[j];}
    const char* name=(longValid&&expected==0&&checksum==shortChecksum(p)&&strlen(longName)<=48)?longName:shortName;
    longValid=false;size_t length=strlen(name);
    if(length<5||name[length-4]!='.'||(name[length-3]|32)!='n'||(name[length-2]|32)!='e'||(name[length-1]|32)!='s')continue;
    bool safe=true;for(size_t j=0;j<length;j++)if(name[j]=='/'||name[j]=='\\'||name[j]==':'||(unsigned char)name[j]<32)safe=false;
    if(!safe||name[0]=='.')continue;
    snprintf(file.name,sizeof(file.name),"%.48s",name);file.size=u32(p+28);file.cluster=u16(p+26);
    return true;
  }
  return false;
}
bool Volume::validChain(const RomFile& file) const {
  if(!data||file.size<16||file.size>bytes)return false;
  uint32_t count=(file.size+clusterBytes-1)/clusterBytes;if(count>clusters)return false;
  uint16_t cluster=file.cluster;
  for(uint32_t i=0;i<count;i++) {if(!validCluster(cluster))return false;cluster=next(cluster);}
  return end(cluster);
}
bool Volume::read(const RomFile& file,uint32_t offset,void* output,size_t length) const {
  if(!data||offset>file.size||length>file.size-offset||(!output&&length))return false;
  uint16_t cluster=file.cluster;uint32_t skip=offset/clusterBytes;
  if(skip>clusters)return false;
  while(skip--) {if(!validCluster(cluster))return false;cluster=next(cluster);}
  uint32_t inside=offset%clusterBytes;uint8_t* target=(uint8_t*)output;
  while(length) {
    if(!validCluster(cluster))return false;
    size_t amount=clusterBytes-inside;if(amount>length)amount=length;
    uint64_t position=(uint64_t)dataOffset+(uint64_t)(cluster-2)*clusterBytes+inside;
    if(position+amount>bytes)return false;
    memcpy(target,data+(size_t)position,amount);target+=amount;length-=amount;inside=0;cluster=next(cluster);
  }
  return true;
}
}
