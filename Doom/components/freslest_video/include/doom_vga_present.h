#pragma once
#include <stdbool.h>
#include <stdint.h>
bool doom_vga_try_present(const uint8_t*,bool,int,int);
void doom_vga_send_frame(const uint8_t*,bool,int,int);
void doom_vga_palette_changed(void);
