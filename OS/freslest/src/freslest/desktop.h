#pragma once
#include <stdint.h>

void desktopBegin();
bool desktopVisible();
bool desktopHasWindow();
void desktopUpdate(uint32_t now);
bool desktopCompose(const uint16_t *appPixels, bool appChanged);
const uint16_t *desktopPixels();
void desktopTerminalEnter();
void desktopTerminalLoop();
