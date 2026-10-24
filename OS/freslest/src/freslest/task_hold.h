#pragma once
#include <stdint.h>
struct TaskHold {
  bool waitingRelease=false,tracking=false;
  uint32_t downAt=0;
  void blockUntilRelease(){waitingRelease=true;tracking=false;}
  bool update(bool down,uint32_t now,uint32_t holdMs) {
    if(!down){waitingRelease=tracking=false;return false;}
    if(waitingRelease)return false;
    if(!tracking){tracking=true;downAt=now;}
    if(now-downAt<holdMs)return false;
    blockUntilRelease();return true;
  }
};
