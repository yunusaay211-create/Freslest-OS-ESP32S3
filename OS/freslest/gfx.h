#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "lcd_profile.h"

// ===== PINS =====
static constexpr int PIN_TFT_MOSI = 11;   // Shared with SD DI.
static constexpr int PIN_TFT_SCK  = 12;   // Shared with SD CLK.
static constexpr int PIN_TFT_CS   = 10;
static constexpr int PIN_TFT_DC   = 9;
static constexpr int PIN_TFT_RST  = 8;
static constexpr int PIN_TFT_BL   = 41;   // BL via MOSFET (PWM on GPIO)
static constexpr int PIN_SD_MISO  = 46;   // SD DO. The TFT remains write-only.
static constexpr int PIN_SD_CS    = 0;


// ===== PERF =====
static constexpr uint32_t SPI_FREQ_WRITE = 27000000; // safer for full-screen line writes
static constexpr uint32_t FRAME_MS = 16;             // ~60fps

// global display
extern lgfx::LGFXBase& tft;

void gfxBegin();
int gfxUiWidth();
int gfxUiHeight();
void gfxSetCompactUi(bool enabled);
void gfxPresent();
void gfxInvalidate();
void gfxMirrorTick();
bool gfxHealthy();
bool frameReady(uint32_t now);


// ===== Backlight (PWM) =====
void backlightInit();
void backlightSet(uint8_t level255);

