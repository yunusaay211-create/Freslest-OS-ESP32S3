#pragma once
#include <stdint.h>
#include <stddef.h>

namespace NesUsbDisk {
constexpr size_t SECTOR_SIZE=512;
constexpr size_t DISK_SIZE=4*1024*1024;
struct RomFile {char name[49];uint32_t size;uint16_t cluster;};
bool format(uint8_t* disk,size_t size,void (*cooperate)()=nullptr,bool doom=false);
// Append a contiguous 8.3 file to a disk created by format(), before USB mounts it.
bool appendFile(uint8_t *disk, size_t size, const char (&shortName)[12], size_t length,
                int (*readFile)(const char *, void *, size_t), const char *path);
class Volume {
 public:
  bool mount(const uint8_t* disk,size_t size);
  bool nextRom(unsigned& index,RomFile& file) const;
  bool validChain(const RomFile& file) const;
  bool read(const RomFile& file,uint32_t offset,void* output,size_t length) const;
 private:
  const uint8_t* data=nullptr;
  size_t bytes=0;
  uint32_t fatOffset=0,rootOffset=0,dataOffset=0,clusterBytes=0,clusters=0,rootEntries=0;
  bool fat12=false;
  uint16_t next(uint16_t cluster) const;
  bool validCluster(uint16_t cluster) const;
  bool end(uint16_t cluster) const;
};
}
