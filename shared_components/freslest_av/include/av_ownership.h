// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {int active,pending,writing;uint32_t swaps;} av_ownership_t;
// Caller serializes these operations. A pending field cannot release the
// previous field until DMA reports the NEW field's EOF, even after prefetch.
void av_ownership_init(av_ownership_t *s);
int av_ownership_acquire(av_ownership_t *s);
bool av_ownership_queue(av_ownership_t *s,int slot);
void av_ownership_cancel(av_ownership_t *s,int slot);
void av_ownership_complete(av_ownership_t *s,int eof_slot);
