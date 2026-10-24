#include "video_slots.h"
void video_slots_init(video_slots_t *s){s->writing=s->ready=s->reading=-1;}
int video_slots_acquire(video_slots_t *s){
 if(s->writing>=0)return -1;
 for(int i=0;i<3;i++)if(i!=s->ready&&i!=s->reading){s->writing=i;return i;}
 return -1;
}
void video_slots_publish(video_slots_t *s,int slot){if(slot>=0&&slot==s->writing){s->ready=slot;s->writing=-1;}}
int video_slots_consume(video_slots_t *s){if(s->ready>=0){s->reading=s->ready;s->ready=-1;}return s->reading;}
