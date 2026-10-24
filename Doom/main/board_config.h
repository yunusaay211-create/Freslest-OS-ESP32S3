#pragma once

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
#define LCD_MADCTL 0xE8
#define LCD_INVERT 1

// Keep an already-wired T_CS inactive; no touchscreen driver or MISO input.
// Composite DAC D0..D7 uses GPIO8/13/14/2/42/40/39/48.
// Never initialize SPI/TFT reset while composite is active.

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
