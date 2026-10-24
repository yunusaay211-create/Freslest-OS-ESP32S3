#ifndef HOST_NVS_H
#define HOST_NVS_H
#include "host_idf.h"
typedef unsigned nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h);
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *p,size_t *n);
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *p,size_t n);
esp_err_t nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);
#endif
