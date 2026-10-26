#pragma once
#include "../../OS/freslest/lcd_profile.h"

// ESP32-S3 N16R8 + ILI9488 SPI
#define LCD_MOSI 11
#define LCD_SCLK 12
#define LCD_CS 10
#define LCD_DC 9
#define LCD_RST 8
#define LCD_BL 41
#define LCD_BL_ON 1

// 40 MHz default: increase to 60 MHz ONLY if panel/wiring is stable.
#define LCD_SPI_HZ (40 * 1000 * 1000)

// Keep the fast 320x240 Doom render; the LCD driver scales it to 480x320.
// Options > FULL SCREEN switches between scaled fullscreen and centered mode.
#define FRESLEST_FAST_VIDEO 1
#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 0
#if FRESLEST_LCD_MODEL == 9341
// Match LovyanGFX rotation 3: MV|MY|ML|BGR. MX would mirror glyphs.
#define LCD_MADCTL 0xB8
#else
#define LCD_MADCTL 0xE8
#endif
#define LCD_INVERT FRESLEST_LCD_INVERT

// Keep an already-wired T_CS inactive; no touchscreen driver or MISO input.
// VGA RGB332 D0..D7 uses GPIO41/8/13/14/2/42/40/39, H=47 V=48.
// Never initialize SPI/TFT reset while VGA is active.

#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_LEFT 6
#define BTN_RIGHT 7
#define BTN_OK 15
#define BTN_BACK 16
#define BTN_FIRE 3

// 0: no automatic map start; open main menu over title screen
#define DOOM_START_IN_LEVEL 0
#define DOOM_START_IN_MENU 1
