#pragma once
#include "freslest_update_package.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*freslest_flash_progress)(unsigned percent, const char *phase);
void freslest_update_task(void *argument);
// Accepts a verified publisher manifest and checks all image bytes before
// touching the inactive OS slot. The caller controls when to reboot.
bool freslest_update_stage_os(const uint8_t *image, size_t size,
                              const freslest_update_package *package,
                              freslest_flash_progress report,
                              char *error, size_t error_size);
#ifdef __cplusplus
}
#endif
