#pragma once
#include <stdint.h>
#define BTN_UP 4
#define BTN_DOWN 5
#define BTN_OK 15
#define BTN_BACK 16
#define BTN_FIRE 3
#ifdef __cplusplus
extern "C" {
#endif
void freslest_update_draw_indexed(const uint8_t* pixels);
#ifdef __cplusplus
}
#endif
