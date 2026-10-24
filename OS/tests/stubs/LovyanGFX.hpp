#pragma once
#include <stdint.h>
namespace lgfx {class LGFX_Device {public: int heightValue=320; int height(){return heightValue;} void startWrite(){} void endWrite(){} void pushImage(int,int,int,int,const uint16_t*);};}
namespace lgfx {using LGFXBase=LGFX_Device;}
extern uint16_t testPixels[480*360];
