#ifndef FRESLEST_SPI_LCD_H
#define FRESLEST_SPI_LCD_H
#include <stdint.h>

void spi_lcd_wait_finish(void);
void spi_lcd_send(uint16_t *scr);
void spi_lcd_init(void);

// Palette entries are Doom RGB565 colors with swapped bytes.
void spi_lcd_set_palette(const int16_t *palette);
// Native 480x320 indexed frame for the USB recovery/update interface.
void spi_lcd_draw_indexed(const uint8_t *frame);

#endif
