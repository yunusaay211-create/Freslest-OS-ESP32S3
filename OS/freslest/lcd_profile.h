#pragma once

// Shared by the OS and Doom. Rebuild BOTH projects after changing this value.
// Original display stays the default; installer builds include both profiles.
#ifndef FRESLEST_LCD_MODEL
#define FRESLEST_LCD_MODEL 9488
#endif

#if FRESLEST_LCD_MODEL == 9341
#define FRESLEST_LCD_WIDTH 320
#define FRESLEST_LCD_HEIGHT 240
#define FRESLEST_LCD_NATIVE_WIDTH 240
#define FRESLEST_LCD_NATIVE_HEIGHT 320
#define FRESLEST_LCD_INVERT 0
#define FRESLEST_LCD_TAG "FRESLEST_LCD:ILI9341"
#elif FRESLEST_LCD_MODEL == 9488
#define FRESLEST_LCD_WIDTH 480
#define FRESLEST_LCD_HEIGHT 320
#define FRESLEST_LCD_NATIVE_WIDTH 320
#define FRESLEST_LCD_NATIVE_HEIGHT 480
#define FRESLEST_LCD_INVERT 1
#define FRESLEST_LCD_TAG "FRESLEST_LCD:ILI9488"
#else
#error "FRESLEST_LCD_MODEL must be 9341 or 9488"
#endif

// Center-sampled nearest neighbour. Keep the logical UI/input/Viewer at
// 480x320; only physical LCD output changes. Both outer corners are sampled.
static inline int freslest_lcd_source_x(int x) {
    return (x * 480 + FRESLEST_LCD_WIDTH / 2) / FRESLEST_LCD_WIDTH;
}
static inline int freslest_lcd_source_y(int y) {
    return (y * 320 + FRESLEST_LCD_HEIGHT / 2) / FRESLEST_LCD_HEIGHT;
}
