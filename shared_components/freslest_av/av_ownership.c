// SPDX-License-Identifier: MIT
#include "av_ownership.h"
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define AV_ISR IRAM_ATTR
#else
#define AV_ISR
#endif
void av_ownership_init(av_ownership_t *s){s->active=0;s->pending=s->writing=-1;s->swaps=0;}
int av_ownership_acquire(av_ownership_t *s){
 if(s->writing>=0)return -1;
 for(int i=0;i<3;i++)if(i!=s->active&&i!=s->pending){s->writing=i;return i;}
 return -1;
}
bool av_ownership_queue(av_ownership_t *s,int slot){
 if(slot<0||slot!=s->writing||s->pending>=0)return false;
 s->pending=slot;s->writing=-1;return true;
}
void av_ownership_cancel(av_ownership_t *s,int slot){if(slot>=0&&s->writing==slot)s->writing=-1;}
void AV_ISR av_ownership_complete(av_ownership_t *s,int slot){
 if(slot>=0&&slot==s->pending){s->active=slot;s->pending=-1;s->swaps++;}
}
