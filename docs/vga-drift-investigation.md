# Horizontal VGA drift investigation — 2026-10-27

User reports continuous left/right rolling in Freslest OS; the previous color-bar core is stable on the same wiring. The checked-in porch/sync/pixel-clock values match that core. Their equivalence makes a timing adjustment an unsupported first change.

The OS previously converted and rewrote all 307,200 physical VGA pixels every UI tick, even for a stationary menu. LCD output already suppressed unchanged frames. The direct PSRAM scanout competes with these CPU reads/writes. ESP-IDF v5.4.4 documents that starvation may permanently desynchronize the DMA read position from LCD output.

Fix candidate: allocate the existing RGB565 shadow in PSRAM for VGA too and reuse content/pointer change detection before conversion. Enable CONFIG_LCD_RGB_RESTART_IN_VSYNC in OS/Doom configs and defaults to limit a transient starvation event to a frame instead of accumulating permanent address drift. Keep working clock, sync, porch and pin values unchanged. Do not add bounce buffers here: this build does not have PSRAM XIP, and bounce mode has cache-disabled flash-write constraints for OTA/NVS.

Evidence: tests/test_lcd_profiles.py now asserts that60 unchanged VGA ticks submit only the first image, changed pixels and pointer movement still submit, and native320x240 remains supported. This failed before the fix and passed afterward. tests/test_vga_transition.py pins VBlank recovery configuration for both projects. Actual hardware verification is still required; do not describe the rolling as physically verified fixed.

Primary references:
- https://github.com/espressif/esp-idf/blob/v5.4.4/docs/en/api-reference/peripherals/lcd/rgb_lcd.rst (PSRAM bandwidth and restart section)
- https://github.com/espressif/esp-idf/blob/v5.4.4/components/esp_lcd/rgb/esp_lcd_panel_rgb.c (lcd_rgb_panel_try_restart_transmission)

Code uploaded to the previously approved public repository/branch after verifying public OS/Doom configs at04a042a4c95e7451af25e0f47f24458aec00e9b3; initial automatic review rejection was resolved by that authorization/destination check. Firmware candidate source915d1107c54a6ade6ba4395bc9cc3821b254a128; physical confirmation still pending.
