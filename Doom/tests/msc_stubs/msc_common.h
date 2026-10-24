#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "diskio_usb.h"
struct usb_disk {uint32_t block_size,block_count;};
typedef struct {usb_disk_t disk;} msc_device_t;
#define __containerof(ptr,type,member) ((type*)((char*)(ptr)-offsetof(type,member)))
#define ESP_ERR_MSC_MOUNT_FAILED 20
#define ESP_ERR_MSC_FORMAT_FAILED 21
#define MSC_RETURN_ON_INVALID_ARG(x) do {if(!(x))return ESP_ERR_INVALID_ARG;}while(0)
#define MSC_RETURN_ON_FALSE(x,e) do {if(!(x))return (e);}while(0)
#define MSC_GOTO_ON_ERROR(x) do {ret=(x);if(ret!=ESP_OK)goto fail;}while(0)
#define MSC_GOTO_ON_FALSE(x,e) do {if(!(x)){ret=(e);goto fail;}}while(0)
