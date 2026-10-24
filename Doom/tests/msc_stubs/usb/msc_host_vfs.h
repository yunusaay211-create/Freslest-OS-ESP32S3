#pragma once
#include "msc_common.h"
#include "ff.h"
typedef void* msc_host_device_handle_t;
typedef struct msc_host_vfs* msc_host_vfs_handle_t;
typedef struct {bool format_if_mount_failed;int max_files;size_t allocation_unit_size;} esp_vfs_fat_mount_config_t;
typedef struct {const char* base_path;const char* fat_drive;int max_files;} esp_vfs_fat_conf_t;
esp_err_t esp_vfs_fat_register_cfg(const esp_vfs_fat_conf_t*,FATFS**);
esp_err_t esp_vfs_fat_unregister_path(const char*);
esp_err_t msc_host_vfs_register(msc_host_device_handle_t,const char*,const esp_vfs_fat_mount_config_t*,msc_host_vfs_handle_t*);
esp_err_t msc_host_vfs_unregister(msc_host_vfs_handle_t);
int msc_host_vfs_get_last_mount_result(void);
