#pragma once
#include <vector>
#include <stdint.h>
#include "esp_timer.h"
extern std::vector<uint16_t> mockDuties;
extern bool mockFailPwm,mockFailCreate,mockFailStart,mockFailWrite;
extern unsigned mockCreated,mockDeleted;
void mockAudioTick(unsigned count);
esp_timer_create_args_t mockCallback();
void mockBlockWrite(bool block);
void mockWaitForBlockedWrite();
