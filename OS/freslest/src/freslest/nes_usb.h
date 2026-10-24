#pragma once
#include <stdint.h>
void nesUsbEnter(bool showScreen=true);
void nesUsbLoop(uint32_t now);
bool nesUsbPoll();
const char* nesUsbStatus();
void nesUsbExit();
