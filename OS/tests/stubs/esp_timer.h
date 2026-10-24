#pragma once
#include <stdint.h>
#include "driver/ledc.h"
#define ESP_TIMER_TASK 0
struct esp_timer;
using esp_timer_handle_t=esp_timer*;
struct esp_timer_create_args_t {void (*callback)(void*);void* arg;int dispatch_method;const char* name;bool skip_unhandled_events;};
esp_err_t esp_timer_create(const esp_timer_create_args_t*,esp_timer_handle_t*);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t,uint64_t);
esp_err_t esp_timer_stop(esp_timer_handle_t);
esp_err_t esp_timer_delete(esp_timer_handle_t);
