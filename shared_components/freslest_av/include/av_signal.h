// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FRESLEST_AV_VERSION "0.1.1"
enum {
 AV_WIDTH=320, AV_HEIGHT=240, AV_LINE=910, AV_LINES=262,
 AV_FIELD_BYTES=AV_LINE*AV_LINES, AV_SAMPLE_HZ=14318181,
 AV_ACTIVE_TOP=20, AV_ACTIVE_START=136, AV_ACTIVE_SAMPLES=744,
 AV_SYNC=0, AV_BLANK=73, AV_BLACK=86, AV_WHITE=255,
 AV_HSYNC=67, AV_EQUALIZE=33, AV_HALF_LINE=455,
 AV_BURST_START=75, AV_BURST_END=115
};
typedef struct {
 uint16_t source[AV_ACTIVE_SAMPLES];
 uint8_t fraction[AV_ACTIVE_SAMPLES];
 uint32_t phases[AV_ACTIVE_SAMPLES];
 uint32_t colors[256];
} av_renderer_t;
// Every sample is stored explicitly: there are no RGB-generated porches.
void av_signal_prime(uint8_t *wave,bool burst);
void av_signal_burst(uint8_t *wave,bool enabled);
void av_renderer_init(av_renderer_t *renderer);
// palette contains four unmodulated colour phases for each RGB332 index.
// detail 0 = interpolated carrier trap; 1 = interpolated sharp output.
void av_signal_encode(av_renderer_t *renderer,uint8_t *wave,const uint8_t *canvas,
                      const uint8_t palette[256][4],unsigned detail);
// 0: monochrome grid/ramp, 1: colour bars, 2: monochrome text/grid, 3: white.
void av_signal_pattern(uint8_t *canvas,unsigned pattern);
void av_signal_palette(uint8_t palette[256][4],int hue,unsigned saturation);
#ifdef __cplusplus
}
#endif
