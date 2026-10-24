#pragma once
#include <stdbool.h>
void usbDoomPoll(void);
bool usbDoomKeyHeld(int);
bool gamepadKeyHeld(int);

// Native 320x240 menu pointer; hidden during first-person gameplay.
bool usbDoomPointer(int *x,int *y);
