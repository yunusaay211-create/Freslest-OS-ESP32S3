// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Value 1 was composite AV and is deliberately never reused.
typedef enum { FRESLEST_DISPLAY_LCD=0, FRESLEST_DISPLAY_VGA=2 } freslest_display_t;
// NVS must already be initialized. Physical BACK during reset forces LCD.
freslest_display_t freslest_video_boot_output(void);
freslest_display_t freslest_video_output(void);
bool freslest_video_save_output(freslest_display_t output);
bool freslest_video_begin(void);
bool freslest_video_healthy(void);
bool freslest_video_starting(void);
void freslest_video_diagnostics(void);
// Same RGB565 source framebuffer as the LCD; input storage is never modified.
void freslest_video_rgb565(const uint16_t *src,int width,int height,bool swapped,
                          bool cursor,int x,int y);
// Doom palette: 0x00BBGGRR.
void freslest_video_indexed(const uint8_t *src,int width,int height,
                           const uint32_t *palette,bool cursor,int x,int y);
#ifdef __cplusplus
}
#endif
