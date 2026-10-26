#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "lcd_profile.h"

// ===== PINS =====
static constexpr int PIN_TFT_MOSI = 11;
static constexpr int PIN_TFT_SCK  = 12;
static constexpr int PIN_TFT_CS   = 10;
static constexpr int PIN_TFT_DC   = 9;
static constexpr int PIN_TFT_RST  = 8;
static constexpr int PIN_TFT_MISO = -1;   // Display is write-only; no touchscreen reads.
static constexpr int PIN_TFT_BL   = 41;   // BL via MOSFET (PWM on GPIO)


// ===== PERF =====
static constexpr uint32_t SPI_FREQ_WRITE = 27000000; // safer for full-screen line writes
static constexpr uint32_t FRAME_MS = 16;             // ~60fps

// global display
extern lgfx::LGFXBase& tft;

void gfxBegin();
int gfxUiHeight();
void gfxPresent();
void gfxMirrorTick();
bool gfxHealthy();
bool frameReady(uint32_t now);


// ===== Backlight (PWM) =====
void backlightInit();
void backlightSet(uint8_t level255);

