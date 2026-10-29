// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Eight physical RGB332 bits, zero-extended into one DMA word per VGA pixel.
void doom_rgb332_palette(const uint8_t *rgb888);
bool doom_rgb332_frame(uint16_t *words,const uint8_t *indices,bool cursor,int x,int y,unsigned fit);
void doom_rgb332_expand(uint16_t *words,size_t count);
