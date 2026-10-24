#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct usb_disk usb_disk_t;
void ff_diskio_register_msc(uint8_t,usb_disk_t*);
uint8_t ff_diskio_get_pdrv_disk(const usb_disk_t*);
unsigned ff_diskio_msc_gpt_partitions(uint8_t);
bool ff_diskio_msc_select_partition(uint8_t,unsigned);
void ff_diskio_unregister_msc(uint8_t);
