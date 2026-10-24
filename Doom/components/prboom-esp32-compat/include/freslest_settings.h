#pragma once
// Volume ranges 0..15; view 3..11; fullscreen 0/1; messages 0/1.
extern int freslest_fullscreen;
void freslest_settings_load(int *sfx, int *music, int *messages, int *view);
void freslest_settings_save(int sfx, int music, int messages, int view);
