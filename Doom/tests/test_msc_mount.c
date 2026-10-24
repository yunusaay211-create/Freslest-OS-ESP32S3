#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msc_scsi_bot.h"
#include "usb/msc_host_vfs.h"
#include "diskio.h"
static FILE *image;static unsigned sector_size,writes,fail_reads,registrations;
static FATFS fs;
esp_err_t scsi_cmd_read10(msc_device_t *dev,uint8_t *out,uint32_t sector,uint32_t count,uint32_t bytes) {
 assert(bytes==sector_size&&(uint64_t)sector+count<=dev->disk.block_count);
 if(fail_reads) {--fail_reads;return ESP_FAIL;}
 assert(fseeko(image,(off_t)sector*bytes,SEEK_SET)==0);return fread(out,bytes,count,image)==count?ESP_OK:ESP_FAIL;
}
esp_err_t scsi_cmd_write10(msc_device_t *dev,const uint8_t *data,uint32_t sector,uint32_t count,uint32_t bytes) {(void)dev;(void)data;(void)sector;(void)count;(void)bytes;++writes;return ESP_FAIL;}
esp_err_t esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t *cfg,FATFS **out) {assert(!strcmp(cfg->base_path,"/usb"));*out=&fs;++registrations;return ESP_OK;}
esp_err_t esp_vfs_fat_unregister_path(const char *path) {(void)path;assert(registrations);--registrations;return ESP_OK;}
void *ff_memalloc(UINT n) {return malloc(n);}
void ff_memfree(void *p) {free(p);}
int ff_mutex_create(int n) {(void)n;return 1;}
void ff_mutex_delete(int n) {(void)n;}
int ff_mutex_take(int n) {(void)n;return 1;}
void ff_mutex_give(int n) {(void)n;}
int main(int argc,char **argv) {
 assert(argc==6);sector_size=atoi(argv[2]);bool good=atoi(argv[3]);unsigned initial_failures=atoi(argv[4]);unsigned cycles=atoi(argv[5]);
 image=fopen(argv[1],"rb");assert(image);fseeko(image,0,SEEK_END);uint64_t size=ftello(image);rewind(image);
 msc_device_t dev={.disk={sector_size,size/sector_size}};
 const esp_vfs_fat_mount_config_t cfg={.format_if_mount_failed=false,.max_files=3};
 for(unsigned cycle=0;cycle<cycles;cycle++) {
  fail_reads=initial_failures;msc_host_vfs_handle_t handle=(void*)0x1234;
  esp_err_t e=msc_host_vfs_register(&dev,"/usb",&cfg,&handle);
  if(good) {
   assert(e==ESP_OK&&handle);FIL file;assert(f_open(&file,"0:/UPDATE.ZIP",FA_READ)==FR_OK);
   char data[24]={0};UINT n;assert(f_read(&file,data,22,&n)==FR_OK&&n==22&&!memcmp(data,"FRESLEST FAT32 USB TEST",22));assert(f_close(&file)==FR_OK);
   assert(msc_host_vfs_unregister(handle)==ESP_OK);
  } else assert(e!=ESP_OK&&handle==NULL);
  assert(!registrations&&!writes);
 }
 fclose(image);puts("PASS: real FatFS mount/read/reinsert and failed-mount cleanup; zero USB writes");return 0;
}
