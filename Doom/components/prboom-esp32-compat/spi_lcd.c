// SPDX-License-Identifier: Apache-2.0
// ILI9488 uses 18-bit RGB (3 bytes/pixel) on the SPI interface.
// Optimized for ESP32-S3:
// - double-buffered DMA
// - asynchronous queued SPI transfers
// - 8 scanlines per transfer
// - per-frame 256-color RGB lookup table

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "board_config.h"
#include "spi_lcd.h"
#include <freslest_video.h>
#include "doom_vga_present.h"
#include <freslest_cursor.h>
#include "usb_doom.h"

// Rendering is 320x240, physical LCD is 480x320.
// Scale RGB palette indices directly into DMA blocks (no full 480x320 buffer).
#define LCD_SRC_W          320
#define LCD_SRC_H          240
#define LCD_PANEL_W        FRESLEST_LCD_WIDTH
#define LCD_PANEL_H        FRESLEST_LCD_HEIGHT
#if FRESLEST_LCD_MODEL == 9341
#define LCD_FIT_W          320
#define LCD_FIT_H          240
#define LCD_BYTES_PER_PIXEL 2
#else
#define LCD_FIT_W          424
#define LCD_FIT_H          318
#define LCD_BYTES_PER_PIXEL 3
#endif
#define LCD_BATCH_LINES    16
#define LCD_LINE_BYTES     (LCD_PANEL_W * LCD_BYTES_PER_PIXEL)
#define LCD_DMA_BYTES      (LCD_LINE_BYTES * LCD_BATCH_LINES)

extern int freslest_fullscreen;
static uint16_t scaled_x[LCD_PANEL_W];
static uint16_t scaled_y[LCD_PANEL_H];
static uint8_t *previous_frame;
static int previous_valid;
static uint32_t palette_generation, previous_palette;

static spi_device_handle_t lcd;

static uint8_t *dma_buf[2];
static spi_transaction_t dma_trans[2];

// Cached 8-bit Doom palette -> ILI9488 RGB888 expansion tables.
// Populated by I_SetPalette(), not regenerated for every frame.
static uint32_t pal_rgb[256]; /* one aligned load per output pixel */

static inline void lcd_pixel(uint8_t *out, uint32_t rgb)
{
#if FRESLEST_LCD_MODEL == 9341
    // RGB888 table uses R in the low byte. SPI RGB565 is high byte first.
    uint16_t c = ((rgb & 0xf8) << 8) | ((rgb & 0xfc00) >> 5) | ((rgb >> 19) & 31);
    out[0] = c >> 8;
    out[1] = c;
#else
    out[0] = rgb;
    out[1] = rgb >> 8;
    out[2] = rgb >> 16;
#endif
}

void spi_lcd_set_palette(const int16_t *palette)
{
    ++palette_generation;
    for (int i = 0; i < 256; ++i) {
        uint16_t sw = (uint16_t)palette[i];
        uint16_t c = (uint16_t)((sw >> 8) | (sw << 8));
        uint32_t r = (((c >> 11) & 31) * 255) / 31;
        uint32_t g = (((c >> 5) & 63) * 255) / 63;
        uint32_t b = ((c & 31) * 255) / 31;
        pal_rgb[i] = r | (g << 8) | (b << 16);
    }
}

void spi_lcd_set_palette_rgb888(const uint8_t *palette){
    // Copy the full original PLAYPAL while its WAD lump is still locked.
    freslest_video_doom_palette(palette);
    doom_vga_palette_changed();
}

static void tx(int dc, const void *data, size_t n)
{
    if (!n) return;

    gpio_set_level(LCD_DC, dc);

    spi_transaction_t t = {
        .length = n * 8,
        .tx_buffer = data
    };

    ESP_ERROR_CHECK(spi_device_transmit(lcd, &t));
}

static void cmd(uint8_t c, const uint8_t *data, size_t n)
{
    tx(0, &c, 1);
    tx(1, data, n);
}

static void window(int x, int y, int w, int h)
{
    uint8_t col[] = {
        x >> 8, x,
        (x + w - 1) >> 8, x + w - 1
    };

    uint8_t row[] = {
        y >> 8, y,
        (y + h - 1) >> 8, y + h - 1
    };

    cmd(0x2a, col, 4);
    cmd(0x2b, row, 4);
    cmd(0x2c, NULL, 0);
}

void spi_lcd_init(void)
{
    if(freslest_video_boot_output()==FRESLEST_DISPLAY_VGA){
        ESP_ERROR_CHECK(freslest_video_begin()?ESP_OK:ESP_FAIL);return;
    }
    puts(FRESLEST_LCD_TAG); // LCD fallback only; VGA startup identifies itself.
    for (int x = 0; x < LCD_FIT_W; ++x)
        scaled_x[x] = (uint16_t)((x * LCD_SRC_W) / LCD_FIT_W);
    for (int y = 0; y < LCD_FIT_H; ++y)
        scaled_y[y] = (uint16_t)((y * LCD_SRC_H) / LCD_FIT_H);
    previous_frame = heap_caps_malloc(LCD_SRC_W * LCD_SRC_H, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    gpio_config_t g = {
        .pin_bit_mask =
            (1ULL << LCD_DC) |
            (1ULL << LCD_RST) |
            (1ULL << LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&g));
    gpio_set_level(LCD_BL, !LCD_BL_ON);

    spi_bus_config_t bus = {
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_DMA_BYTES
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO)
    );

    spi_device_interface_config_t dev = {
        .clock_speed_hz = LCD_SPI_HZ,
        .mode = 0,
        .spics_io_num = LCD_CS,
        .queue_size = 2
    };

    ESP_ERROR_CHECK(
        spi_bus_add_device(SPI2_HOST, &dev, &lcd)
    );

    dma_buf[0] = heap_caps_malloc(
        LCD_DMA_BYTES,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    dma_buf[1] = heap_caps_malloc(
        LCD_DMA_BYTES,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    assert(dma_buf[0]);
    assert(dma_buf[1]);

    memset(dma_trans, 0, sizeof(dma_trans));

    gpio_set_level(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    cmd(0x01, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(150));

#if FRESLEST_LCD_MODEL == 9341
    // ILI9341 controller-specific power/timing setup (16-bit SPI pixels).
    cmd(0xef, (uint8_t[]){0x03,0x80,0x02}, 3);
    cmd(0xcf, (uint8_t[]){0x00,0xc1,0x30}, 3);
    cmd(0xed, (uint8_t[]){0x64,0x03,0x12,0x81}, 4);
    cmd(0xe8, (uint8_t[]){0x85,0x00,0x78}, 3);
    cmd(0xcb, (uint8_t[]){0x39,0x2c,0x00,0x34,0x02}, 5);
    cmd(0xf7, (uint8_t[]){0x20}, 1);
    cmd(0xea, (uint8_t[]){0x00,0x00}, 2);
    cmd(0xc0, (uint8_t[]){0x23}, 1);
    cmd(0xc1, (uint8_t[]){0x10}, 1);
    cmd(0xc5, (uint8_t[]){0x3e,0x28}, 2);
    cmd(0xc7, (uint8_t[]){0x86}, 1);
    cmd(0x36, (uint8_t[]){LCD_MADCTL}, 1);
    cmd(0x3a, (uint8_t[]){0x55}, 1);
    cmd(0xb1, (uint8_t[]){0x00,0x13}, 2);
    cmd(0xb6, (uint8_t[]){0x08,0xc2,0x27}, 3);
    cmd(0xf2, (uint8_t[]){0x00}, 1);
    cmd(0x26, (uint8_t[]){0x01}, 1);
#else
    const uint8_t pg[] = {
        0x00,0x03,0x09,0x08,0x16,
        0x0a,0x3f,0x78,0x4c,0x09,
        0x0a,0x08,0x16,0x1a,0x0f
    };

    const uint8_t ng[] = {
        0x00,0x16,0x19,0x03,0x0f,
        0x05,0x32,0x45,0x46,0x04,
        0x0e,0x0d,0x35,0x37,0x0f
    };

    cmd(0xe0, pg, sizeof(pg));
    cmd(0xe1, ng, sizeof(ng));

    cmd(0xc0, (uint8_t[]){0x17,0x15}, 2);
    cmd(0xc1, (uint8_t[]){0x41}, 1);
    cmd(0xc5, (uint8_t[]){0x00,0x12,0x80}, 3);

    cmd(0x36, (uint8_t[]){LCD_MADCTL}, 1);

    // 18-bit / 3-byte SPI pixel mode
    cmd(0x3a, (uint8_t[]){0x66}, 1);

    cmd(0xb0, (uint8_t[]){0x00}, 1);
    cmd(0xb1, (uint8_t[]){0xa0}, 1);
    cmd(0xb4, (uint8_t[]){0x02}, 1);
    cmd(0xb6, (uint8_t[]){0x02,0x02}, 2);
    cmd(0xe9, (uint8_t[]){0x00}, 1);
    cmd(0xf7, (uint8_t[]){0xa9,0x51,0x2c,0x82}, 4);

#endif
    cmd(LCD_INVERT ? 0x21 : 0x20, NULL, 0);

    cmd(0x11, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(150));

    cmd(0x29, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Brief RGB bars verify the physical display before the engine starts.
    uint8_t *line = dma_buf[0];

    window(0, 0, LCD_PANEL_W, LCD_PANEL_H);

    for (int x = 0; x < LCD_PANEL_W; x++) {
        uint32_t rgb = x < LCD_PANEL_W / 3 ? 0x0000ff :
                       x < LCD_PANEL_W * 2 / 3 ? 0x00ff00 : 0xff0000;
        lcd_pixel(line + x * LCD_BYTES_PER_PIXEL, rgb);
    }

    for (int y = 0; y < LCD_PANEL_H; y++) {
        tx(1, line, LCD_LINE_BYTES);
    }

    gpio_set_level(LCD_BL, LCD_BL_ON);
    vTaskDelay(pdMS_TO_TICKS(700));

    memset(line, 0, LCD_LINE_BYTES);

    window(0, 0, LCD_PANEL_W, LCD_PANEL_H);

    for (int y = 0; y < LCD_PANEL_H; y++) {
        tx(1, line, LCD_LINE_BYTES);
    }

    ESP_LOGI(
        "LCD",
        "ILI%d ready: Doom %dx%d -> LCD %dx%d, SPI %d Hz, queued DMA",
        FRESLEST_LCD_MODEL, LCD_SRC_W, LCD_SRC_H, LCD_PANEL_W, LCD_PANEL_H, LCD_SPI_HZ
    );
}

void spi_lcd_wait_finish(void)
{
    // spi_lcd_send() drains all queued transfers before returning.
}

void spi_lcd_draw_indexed(const uint8_t *frame)
{
    if(freslest_video_output()==FRESLEST_DISPLAY_VGA){freslest_video_indexed(frame,480,320,pal_rgb,false,0,0);return;}
    window(0, 0, LCD_PANEL_W, LCD_PANEL_H);
    for (int y = 0; y < LCD_PANEL_H; y += LCD_BATCH_LINES) {
        for (int yy = 0; yy < LCD_BATCH_LINES; ++yy) {
            int sy = freslest_lcd_source_y(y + yy);
            for (int x = 0; x < LCD_PANEL_W; ++x) {
                uint32_t color = pal_rgb[frame[sy * 480 + freslest_lcd_source_x(x)]];
                lcd_pixel(dma_buf[0] + (yy * LCD_PANEL_W + x) * LCD_BYTES_PER_PIXEL, color);
            }
        }
        tx(1, dma_buf[0], LCD_DMA_BYTES);
    }
    previous_valid = 0;
}

void spi_lcd_send(uint16_t *scr)
{
    if(freslest_video_output()==FRESLEST_DISPLAY_VGA){
        int x,y;bool cursor=usbDoomPointer(&x,&y);
        doom_vga_send_frame((const uint8_t*)scr,cursor,x,y);return;
    }
    static int last_fullscreen = -1;
    static int old_cursor,old_cursor_y;
    int cursor_x,cursor_y;
    bool cursor=usbDoomPointer(&cursor_x,&cursor_y);
    // Doom provides an 8-bit palette-index image, despite the historical
    // uint16_t argument type of spi_lcd_send().
    const uint8_t *src = (const uint8_t *)scr;
    const int fullscreen = (freslest_fullscreen != 0);
    const int out_w = fullscreen ? LCD_FIT_W : LCD_SRC_W;
    const int out_h = fullscreen ? LCD_FIT_H : LCD_SRC_H;
    const int start_x = (LCD_PANEL_W - out_w) / 2;
    const int start_y = (LCD_PANEL_H - out_h) / 2;
    const int line_bytes = out_w * LCD_BYTES_PER_PIXEL;

    // After switching from full screen to centered, clear the old image
    // outside the new 320x240 window once. Otherwise stale pixels remain.
    if (last_fullscreen != fullscreen) {
        previous_valid = 0;
        window(0, 0, LCD_PANEL_W, LCD_PANEL_H);
        memset(dma_buf[0], 0, LCD_DMA_BYTES);
        for (int y = 0; y < LCD_PANEL_H; y += LCD_BATCH_LINES)
            tx(1, dma_buf[0], LCD_DMA_BYTES);
    }
    last_fullscreen = fullscreen;
    int first_y = 0, end_y = out_h;
    if (previous_frame && previous_valid && previous_palette == palette_generation) {
        int first_source = LCD_SRC_H, last_source = -1;
        for (int y = 0; y < LCD_SRC_H; ++y) {
            if ((old_cursor&&y>=old_cursor_y&&y<old_cursor_y+FRESLEST_CURSOR_H)||
                (cursor&&y>=cursor_y&&y<cursor_y+FRESLEST_CURSOR_H)||
                memcmp(previous_frame + y * LCD_SRC_W, src + y * LCD_SRC_W, LCD_SRC_W)) {
                if (first_source == LCD_SRC_H) first_source = y;
                last_source = y;
            }
        }
        if (last_source < 0) { taskYIELD(); return; }
        if (fullscreen) {
            while (first_y < out_h && scaled_y[first_y] < first_source) ++first_y;
            end_y = first_y;
            while (end_y < out_h && scaled_y[end_y] <= last_source) ++end_y;
        } else { first_y = first_source; end_y = last_source + 1; }
    }
    old_cursor=cursor;old_cursor_y=cursor_y;
    window(start_x, start_y + first_y, out_w, end_y - first_y);
    gpio_set_level(LCD_DC, 1);

    int queued = 0;
    int chunk = 0;
    for (int y = first_y; y < end_y; y += LCD_BATCH_LINES, ++chunk) {
        int idx = chunk & 1;
        if (queued == 2) {
            spi_transaction_t *done = NULL;
            ESP_ERROR_CHECK(spi_device_get_trans_result(lcd, &done, portMAX_DELAY));
            queued--;
        }
        int lines = end_y - y;
        if (lines > LCD_BATCH_LINES) lines = LCD_BATCH_LINES;
        uint8_t *dst = dma_buf[idx];
        int last_src_y = -1;
        for (int yy = 0; yy < lines; ++yy) {
            const int src_y = fullscreen ? scaled_y[y + yy] : y + yy;
            const uint8_t *row = src + src_y * LCD_SRC_W;
            uint8_t *out = dst + yy * line_bytes;
            /* Repeated nearest-neighbor rows are already RGB-expanded.
             * Copy inside the currently owned DMA buffer: never read a buffer
             * still owned by the SPI peripheral. */
            if (src_y == last_src_y) {
                memcpy(out, out - line_bytes, line_bytes);
                continue;
            }
            last_src_y = src_y;
            for (int x = 0; x < out_w; ++x) {
                const uint8_t p = row[fullscreen ? scaled_x[x] : x];
                const int o = x * LCD_BYTES_PER_PIXEL;
                uint32_t rgb = pal_rgb[p];
                if(cursor){unsigned c=freslest_cursor_pixel((fullscreen?scaled_x[x]:x)-cursor_x,src_y-cursor_y);
                    if(c)rgb=c==1?0:c==2?0xffffff:0xffa000;}
                lcd_pixel(out + o, rgb);
            }
        }
        memset(&dma_trans[idx], 0, sizeof(spi_transaction_t));
        dma_trans[idx].length = (size_t)lines * line_bytes * 8;
        dma_trans[idx].tx_buffer = dst;
        ESP_ERROR_CHECK(spi_device_queue_trans(lcd, &dma_trans[idx], portMAX_DELAY));
        queued++;
    }

    // Drain the final queued DMA transfers.
    while (queued > 0) {

        spi_transaction_t *done = NULL;

        ESP_ERROR_CHECK(
            spi_device_get_trans_result(
                lcd,
                &done,
                portMAX_DELAY
            )
        );

        queued--;
    }

    if (previous_frame) {
        memcpy(previous_frame, src, LCD_SRC_W * LCD_SRC_H);
        previous_valid = 1;
        previous_palette = palette_generation;
    }
    // No forced 1-tick delay here.
    // Waiting for queued DMA results already gives the scheduler opportunities.
    taskYIELD();
}
