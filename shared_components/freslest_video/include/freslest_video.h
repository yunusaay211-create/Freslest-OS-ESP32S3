// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { FRESLEST_DISPLAY_LCD=0, FRESLEST_DISPLAY_COMPOSITE=1 } freslest_display_t;
typedef struct { int8_t x,y; uint8_t scale; } freslest_tv_config_t;
enum { FRESLEST_TV_CLEAN=0, FRESLEST_TV_SHARP=1 };
typedef struct { int16_t hue; uint8_t saturation,detail; } freslest_tv_color_t;
// NVS must already be initialized. Holding physical BACK at boot forces LCD.
freslest_display_t freslest_video_boot_output(void);
freslest_display_t freslest_video_output(void);
bool freslest_video_save_output(freslest_display_t output);
bool freslest_video_begin(void);
bool freslest_video_healthy(void);
// A bounded first-frame wait, never a successful health result. Once a frame
// has been shown, later sync failures must reach the normal rollback check.
bool freslest_video_starting(void);
freslest_tv_config_t freslest_video_tv_config(void);
void freslest_video_tv_preview(freslest_tv_config_t config);
bool freslest_video_tv_save(freslest_tv_config_t config);
freslest_tv_color_t freslest_video_color_config(void);
void freslest_video_color_preview(freslest_tv_color_t config);
bool freslest_video_color_save(freslest_tv_color_t config);
void freslest_video_diagnostics(void);
// Inputs are copied before return. Single producer, separate VSYNC consumer.
void freslest_video_rgb565(const uint16_t *src,int width,int height,bool swapped,
                          bool cursor,int x,int y);
// Palette is 0x00BBGGRR, matching Doom's native palette cache.
void freslest_video_indexed(const uint8_t *src,int width,int height,
                           const uint32_t *palette,bool cursor,int x,int y);
#ifdef __cplusplus
}
#endif
