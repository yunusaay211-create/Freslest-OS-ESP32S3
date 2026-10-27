# LCD/VGA transition implementation plan

Goal: remove composite AV and integrate the user's provided RGB332 VGA V1.0 timing in both OS and Doom, keeping native ILI9341 menus and rebuilding the installer.
Architecture: replace the shared video backend with ESP-IDF RGB LCD output, 640x480/8-bit PSRAM framebuffer. Keep source RGB565 and Doom indexed conversion separate from transport. VGA mode owns shared LCD pins for the whole boot; selection requires restart.
Tech: existing ESP-IDF v5.4.4, esp_lcd RGB, C/C++, Python host regression tests and existing Windows CI.
User authorization: 2026-10-26 'onaylıyom ve av;yi kaldır ... yeni realese notu yaz'; use provided V1.0 core and prior VGA conversation. Do not publish a release or change main.

- [x] Test actual VGA initialization configuration, pixel conversion, failure cleanup, boot selection/migration and health grace. Watch fail before replacing backend.
- [x] Remove AV engine, TV alignment/color controls and lab boot path. Add LCD/VGA/USB Viewer selection. Ignore old composite NVS selection; BACK+reset restores LCD.
- [ ] Test menu selection and pointer targets at both LCD sizes; test OS/Doom bypass without touching shared LCD pins; confirm firmware partition fit and Windows GUI smoke tests in existing CI.
- [ ] Update source/archive and draft release notes. Describe RGB565 source to physical RGB332 (256 colors), shared-pin wiring and experimental signal stability honestly. Keep release number 1.0 until user chooses a new number.

Review focus: no LCD/LEDC initialization after VGA claims GPIO8/41; failed panel setup must release resources; old AV output value must not silently activate VGA; swapped RGB565 and Doom palette must produce matching RGB332; stalled VSYNC after healthy boot must reach OTA health failure.

2026-10-27 review: independent reviewer could not run (service usage limit). Author review found excessive converter stack usage (1408 bytes on host); a failing stack-budget regression was added, coordinate scratch moved outside the OS task stack, then test passed. Strict misleading-indentation warning reproduced and fixed; host tests now keep that warning enabled. Firmware/Windows CI restarted at b17193d5cc0ca8905789501b80147b14e685929f.
