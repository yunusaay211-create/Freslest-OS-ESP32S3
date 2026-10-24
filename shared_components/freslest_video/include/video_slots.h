#pragma once
// Caller holds a short cross-core critical section around these operations.
typedef struct {int writing,ready,reading;} video_slots_t;
void video_slots_init(video_slots_t *s);
int video_slots_acquire(video_slots_t *s);
void video_slots_publish(video_slots_t *s,int slot);
int video_slots_consume(video_slots_t *s);
