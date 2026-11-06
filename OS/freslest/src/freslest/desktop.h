#pragma once
#include <stdint.h>

namespace desktop_color {
constexpr uint16_t Panel = 0xd71f, Border = 0x8cfa, Ink = 0x1951, Selected = 0xbf3f;
}

void desktopBegin();
bool desktopVisible();
bool desktopHasWindow();
bool desktopCloseModal();
void desktopUpdate(uint32_t now);
bool desktopCompose(const uint16_t *appPixels, bool appChanged);
const uint16_t *desktopPixels();
void desktopTerminalEnter();
void desktopTerminalLoop();
