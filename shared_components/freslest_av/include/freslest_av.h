// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "av_signal.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct freslest_av *freslest_av_handle_t;
typedef bool (*freslest_av_field_cb)(void *context);
typedef struct {freslest_av_field_cb on_field;void *context;} freslest_av_config_t;
typedef struct {
 uint32_t fields,swaps,descriptor_errors,unknown_eof,last_field_us,last_period_us,max_period_us,sample_hz;
 int active,pending,writing;
 bool running;
} freslest_av_stats_t;
extern const int freslest_av_gpio[8];
esp_err_t freslest_av_open(const freslest_av_config_t *config,freslest_av_handle_t *out);
void freslest_av_close(freslest_av_handle_t engine);
// Only a returned WRITING field may be modified. Publish can return false
// while an earlier field is pending; retain the field and retry later.
int freslest_av_acquire(freslest_av_handle_t engine,uint8_t **wave);
bool freslest_av_publish(freslest_av_handle_t engine,int slot);
void freslest_av_cancel(freslest_av_handle_t engine,int slot);
void freslest_av_stats(freslest_av_handle_t engine,freslest_av_stats_t *stats);
bool freslest_av_healthy(freslest_av_handle_t engine);
bool freslest_av_lab_requested(void);
void freslest_av_lab_run(int (*read_key)(void));
#ifdef __cplusplus
}
#endif
