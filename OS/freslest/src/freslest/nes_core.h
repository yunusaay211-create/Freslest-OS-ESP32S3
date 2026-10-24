#pragma once
#include <stdint.h>
bool nesCoreStart(const char* filename);
void nesCoreStop();
void nesCoreFrame(bool draw,uint8_t pad);
bool nesCoreMapperSupported(unsigned mapper);
void nesCoreSetFullscreen(bool enabled);
