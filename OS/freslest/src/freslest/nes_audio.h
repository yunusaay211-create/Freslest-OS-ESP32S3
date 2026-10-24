#pragma once
#include <stddef.h>
#include <stdint.h>
struct NesAudioStats {
  bool running;
  size_t queued;
  uint32_t dropped,underruns,hardwareErrors;
};
bool nesAudioStart();
void nesAudioStop();
void nesAudioSetVolume(bool enabled,unsigned volume255);
void nesAudioSubmit(const int16_t* pcm,size_t count);
NesAudioStats nesAudioStats();
