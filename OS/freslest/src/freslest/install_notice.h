#pragma once
#include <stdint.h>

// Call on the UI task after the OTA startup check. True means repaint subtitle.
bool installNoticeUpdate(uint32_t now,bool startupReady,bool headerVisible);
const char* installNoticeText();
