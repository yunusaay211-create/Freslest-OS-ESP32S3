#pragma once
#include <Arduino.h>

struct NesRomInfo {
  String name;
  String path;
  uint8_t mapper = 0;
  uint16_t prgKB = 0;
  uint16_t chrKB = 0;
  bool valid = false;
  String error;
};

void nesLibraryEnter();
void nesLibraryLoop(uint32_t now);
void nesLibraryRefresh();

void nesPlayerExit();
bool nesPlayerFullscreen();
void nesPlayerEnter();
void nesPlayerLoop(uint32_t now);

const NesRomInfo* nesSelectedRom();
