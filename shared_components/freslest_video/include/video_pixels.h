#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "freslest_video.h"
#define VIDEO_W 320
#define VIDEO_H 240
#define VIDEO_SAMPLES 14318181
// NTSC: 227.5 carrier cycles per line at four samples per cycle. The
// half-cycle advances burst AND active chroma by 180 degrees on odd lines.
#define VIDEO_LINE_SAMPLES 910
#define VIDEO_FIELD_LINES 262
#define VIDEO_DMA_W 900
#define VIDEO_DMA_H 252
#define VIDEO_IMAGE_TOP 6
#define VIDEO_IMAGE_START 136
#define VIDEO_IMAGE_SAMPLES 744
#define VIDEO_BLANK 73
#define VIDEO_BLACK 86
void video_phase_palette(uint8_t palette[256][4]);
void video_color_palette(uint8_t palette[256][4],const freslest_tv_color_t *config);
void video_prime_wave(uint8_t *wave);
void video_burst_wave(uint8_t *wave,bool enabled);
void video_encode_wave(uint8_t *wave,const uint8_t *pixels,const uint8_t palette[256][4]);
void video_encode_wave_detail(uint8_t *wave,const uint8_t *pixels,const uint8_t palette[256][4],unsigned detail);
void video_convert(uint8_t *dest,const void *source,int width,int height,
                   const uint32_t *palette,bool swapped,bool cursor,int x,int y,
                   const freslest_tv_config_t *config);
