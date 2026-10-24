#pragma once
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(n) (n)
static inline void vTaskDelay(unsigned n) {(void)n;}
