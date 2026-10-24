#pragma once
#include <assert.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_FOUND 2
#define ESP_INTR_FLAG_LEVEL1 1
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_NVS_NOT_FOUND 4
typedef struct {uint32_t address,size;unsigned type,subtype;const char* label;} esp_partition_t;
typedef struct {char project_name[32],version[32];} esp_app_desc_t;
typedef enum {ESP_OTA_IMG_NEW,ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_VALID,ESP_OTA_IMG_INVALID,ESP_OTA_IMG_ABORTED} esp_ota_img_states_t;
typedef unsigned esp_ota_handle_t;
typedef struct {void (*callback)(void*);void *arg;const char *name;} esp_timer_create_args_t;
typedef struct {void (*callback)(void*);void *arg;} test_timer;
typedef test_timer* esp_timer_handle_t;
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_ANY 255
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 16
#define ESP_PARTITION_SUBTYPE_APP_OTA_1 17
#define ESP_PARTITION_SUBTYPE_APP_OTA_2 18

#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_FACTORY 0
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
typedef int nvs_handle_t;
#define NVS_READWRITE 1
#define NVS_READONLY 0
typedef unsigned TickType_t;
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(n) (n)
#define pdPASS 1
#define pdTRUE 1
#define USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS 1
typedef void *QueueHandle_t;
typedef void *msc_host_device_handle_t;
typedef void *msc_host_vfs_handle_t;
typedef struct {enum {MSC_DEVICE_CONNECTED,MSC_DEVICE_DISCONNECTED} event;union {uint8_t address;void *handle;} device;} msc_host_event_t;
typedef struct {int intr_flags;} usb_host_config_t;
typedef struct {bool create_backround_task;unsigned task_priority,stack_size;int core_id;void (*callback)(const msc_host_event_t*,void*);} msc_host_driver_config_t;
typedef struct {bool format_if_mount_failed;int max_files;size_t allocation_unit_size;} esp_vfs_fat_mount_config_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en;} gpio_config_t;
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_ENABLE 1

extern uint8_t mock_mode;
extern bool mock_nvs_fail,mock_heap_fail,mock_bad_running,mock_boot_fail,mock_read_corrupt;
extern unsigned mock_allocations,mock_erases,mock_writes,mock_unmounts,mock_first_erase_mode;
extern int mock_write_failure;
extern uint8_t *mock_flash;
extern esp_partition_t mock_parts[6];
#define mock_os mock_parts[1]
#define mock_doom mock_parts[0]
extern jmp_buf mock_restart;
extern unsigned mock_running,mock_ota_states[3],mock_events,mock_cut_at;
extern uint8_t mock_saved[32],mock_staged[32];extern size_t mock_saved_size,mock_staged_size;
extern int64_t mock_time;extern bool mock_end_fail,mock_begin_fail;
extern void *mock_heap_block;
static inline void mock_event(void) {if(++mock_events==mock_cut_at)longjmp(mock_restart,2);}

extern const esp_partition_t *mock_boot;
extern jmp_buf mock_restart;

static inline const char *esp_err_to_name(esp_err_t e) {(void)e;return "TEST ERROR";}
static inline esp_err_t nvs_flash_init(void) {return ESP_OK;}
static inline esp_err_t nvs_open(const char *name,int access,nvs_handle_t *h) {(void)name;(void)access;*h=1;return ESP_OK;}
static inline esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *v,size_t *n) {(void)h;(void)key;if(!mock_saved_size)return ESP_ERR_NVS_NOT_FOUND;assert(*n>=mock_saved_size);*n=mock_saved_size;memcpy(v,mock_saved,*n);return ESP_OK;}
static inline esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *v,size_t n) {(void)h;(void)key;if(mock_nvs_fail)return ESP_FAIL;assert(n<=32);memcpy(mock_staged,v,n);mock_staged_size=n;return ESP_OK;}
static inline esp_err_t nvs_commit(nvs_handle_t h) {(void)h;mock_event();if(mock_nvs_fail)return ESP_FAIL;memcpy(mock_saved,mock_staged,mock_staged_size);mock_saved_size=mock_staged_size;mock_mode=mock_saved[5];mock_event();return ESP_OK;}
static inline void nvs_close(nvs_handle_t h) {(void)h;}
static inline const esp_partition_t *esp_partition_find_first(int type,int subtype,const char *name) {
 for(unsigned i=0;i<6;i++)if(mock_parts[i].type==(unsigned)type&&(subtype==255||mock_parts[i].subtype==(unsigned)subtype)&&(!name||!strcmp(name,mock_parts[i].label)))return &mock_parts[i];return NULL;
}
static inline const esp_partition_t *esp_ota_get_running_partition(void) {return mock_bad_running?&mock_parts[2]:&mock_parts[mock_running];}
static inline const esp_partition_t *esp_ota_get_boot_partition(void) {return mock_boot;}
static inline esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *out) {assert(p->subtype>=16&&p->subtype<=18);*out=mock_ota_states[p->subtype-16];return ESP_OK;}
static inline esp_err_t esp_ota_get_partition_description(const esp_partition_t *p,esp_app_desc_t *out) {(void)p;memset(out,0,sizeof(*out));strcpy(out->project_name,"freslest_os_s3");strcpy(out->version,"5.0.0");return ESP_OK;}
static inline esp_err_t esp_partition_get_sha256(const esp_partition_t *p,uint8_t *hash) {(void)p;memset(hash,0,32);return ESP_OK;}
static inline esp_err_t esp_timer_create(const esp_timer_create_args_t *args,esp_timer_handle_t *out) {static test_timer t;t.callback=args->callback;t.arg=args->arg;*out=&t;return ESP_OK;}
static inline esp_err_t esp_timer_start_once(esp_timer_handle_t t,uint64_t us) {(void)t;assert(us==45000000);return ESP_OK;}
static inline esp_err_t esp_timer_stop(esp_timer_handle_t t) {(void)t;return ESP_OK;}
static inline esp_err_t esp_timer_delete(esp_timer_handle_t t) {(void)t;return ESP_OK;}
static inline esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) {mock_event();mock_ota_states[mock_running]=ESP_OTA_IMG_VALID;mock_event();return ESP_OK;}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p);
static inline esp_err_t esp_partition_erase_range(const esp_partition_t *p,uint32_t offset,size_t bytes) {
 assert(p==&mock_os&&offset+bytes<=p->size&&offset%4096==0&&bytes==4096);
 assert(mock_unmounts==2&&mock_mode==2);mock_event();
 if(!mock_erases)mock_first_erase_mode=mock_mode;
 ++mock_erases;memset(mock_flash+offset,0xff,bytes);mock_event();return ESP_OK;
}
static inline esp_err_t esp_partition_write(const esp_partition_t *p,uint32_t offset,const void *data,size_t bytes) {
 assert(p==&mock_os&&offset+bytes<=p->size);
 if(mock_write_failure>=0&&(int)mock_writes>=mock_write_failure)return ESP_FAIL;
 mock_event();++mock_writes;memcpy(mock_flash+offset,data,bytes);mock_event();return ESP_OK;
}
static inline esp_err_t esp_partition_read(const esp_partition_t *p,uint32_t offset,void *data,size_t bytes) {
 assert(p==&mock_os&&offset+bytes<=p->size);mock_event();memcpy(data,mock_flash+offset,bytes);mock_event();
 if(mock_read_corrupt&&bytes)((uint8_t*)data)[0]^=1;return ESP_OK;
}
static inline void *heap_caps_malloc(size_t n,int caps) {(void)caps;if(mock_heap_fail)return NULL;void *p=malloc(n);if(p) {++mock_allocations;mock_heap_block=p;};return p;}
static inline void heap_caps_free(void *p) {if(p) {assert(mock_allocations);--mock_allocations;mock_heap_block=NULL;free(p);}}
static inline esp_err_t msc_host_vfs_unregister(void *h) {(void)h;++mock_unmounts;return ESP_OK;}
static inline esp_err_t msc_host_uninstall_device(void *h) {(void)h;++mock_unmounts;return ESP_OK;}
static inline esp_err_t msc_host_install_device(uint8_t address,void **h) {(void)address;*h=(void*)1;return ESP_OK;}
static inline esp_err_t msc_host_vfs_register(void *h,const char *path,const esp_vfs_fat_mount_config_t *cfg,void **out) {(void)h;(void)path;(void)cfg;*out=(void*)1;return ESP_OK;}
static inline QueueHandle_t xQueueCreate(unsigned n,size_t size) {(void)n;(void)size;return (void*)1;}
static inline int xQueueSend(void *q,const void *p,unsigned wait) {(void)q;(void)p;(void)wait;return 1;}
static inline int xQueueReceive(void *q,void *p,unsigned wait) {(void)q;(void)p;(void)wait;return 0;}
static inline int xTaskCreatePinnedToCore(void (*f)(void*),const char *s,unsigned stack,void *p,unsigned priority,void *handle,int core) {(void)f;(void)s;(void)stack;(void)p;(void)priority;(void)handle;(void)core;return pdPASS;}
static inline void vTaskDelay(unsigned n) {(void)n;}
static inline void vTaskDelete(void *p) {(void)p;}
static inline esp_err_t usb_host_install(const usb_host_config_t *c) {(void)c;return ESP_OK;}
static inline esp_err_t usb_host_uninstall(void) {return ESP_OK;}
static inline esp_err_t msc_host_install(const msc_host_driver_config_t *c) {(void)c;return ESP_OK;}
static inline esp_err_t msc_host_uninstall(void) {return ESP_OK;}
static inline esp_err_t usb_host_device_free_all(void) {return ESP_OK;}
static inline void usb_host_lib_handle_events(unsigned wait,uint32_t *flags) {(void)wait;*flags=0;}
static inline int64_t esp_timer_get_time(void) {return mock_time;}
static inline void gpio_config(const gpio_config_t *c) {(void)c;}
static inline int gpio_get_level(int pin) {(void)pin;return 1;}
static inline _Noreturn void esp_restart(void) {longjmp(mock_restart,1);}
static size_t mock_ota_offset;
static inline esp_err_t esp_ota_begin(const esp_partition_t *p,size_t n,esp_ota_handle_t *h) {
 if(mock_begin_fail)return ESP_FAIL;assert(p==&mock_os);mock_ota_offset=0;*h=1;
 for(size_t at=0;at<((n+4095)&~4095u);at+=4096)esp_partition_erase_range(p,at,4096);return ESP_OK;
}
static inline esp_err_t esp_ota_write(esp_ota_handle_t h,const void *p,size_t n) {assert(h==1);esp_err_t e=esp_partition_write(&mock_os,mock_ota_offset,p,n);if(e==ESP_OK)mock_ota_offset+=n;return e;}
static inline esp_err_t esp_ota_end(esp_ota_handle_t h) {assert(h==1);mock_event();return mock_end_fail?ESP_FAIL:ESP_OK;}
static inline esp_err_t esp_ota_abort(esp_ota_handle_t h) {(void)h;return ESP_OK;}
typedef struct {uint32_t sector_count,sector_size;uint16_t idVendor,idProduct;} msc_host_device_info_t;
static inline esp_err_t msc_host_get_device_info(void* h,msc_host_device_info_t* out) {(void)h;*out=(msc_host_device_info_t){.sector_count=70000,.sector_size=512};return ESP_OK;}
static inline int msc_host_vfs_get_last_mount_result(void) {return 13;}
typedef SHA256_CTX mbedtls_sha256_context;
static inline void mbedtls_sha256_init(mbedtls_sha256_context *c) {memset(c,0,sizeof(*c));}
static inline void mbedtls_sha256_free(mbedtls_sha256_context *c) {(void)c;}
static inline int mbedtls_sha256_starts(mbedtls_sha256_context *c,int mode) {(void)mode;return SHA256_Init(c)?0:-1;}
static inline int mbedtls_sha256_update(mbedtls_sha256_context *c,const void *p,size_t n) {return SHA256_Update(c,p,n)?0:-1;}
static inline int mbedtls_sha256_finish(mbedtls_sha256_context *c,uint8_t out[32]) {return SHA256_Final(out,c)?0:-1;}
static inline int mbedtls_sha256(const void *p,size_t n,uint8_t out[32],int mode) {(void)mode;return SHA256(p,n,out)?0:-1;}
