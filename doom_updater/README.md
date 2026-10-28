# Freslest Doom VGA updater 1.1

This executable updates only the existing Doom app partition. Connect the ESP32-S3 programming USB, choose its COM port, and click **Doom’u güncelle**. Then open Doom from Freslest. OS firmware, saved display settings, WAD, files and saves are preserved.

Doom renders 320x240 and outputs the existing 640x480 / 25.175 MHz VGA signal with the saved fit. The local backend is independent of OS video code. Static menus no longer repeatedly overwrite PSRAM scanout frames; palette/cursor/source changes redraw, failed submissions retry, and render submissions are capped at 30 FPS. This does not establish that the reported physical red tint is fixed.

The updater requires the Freslest A/B v2 layout: Doom at 0x20000/0xe0000, OS A at 0x100000/0x1b0000, OS B at 0x2b0000/0x1b0000. A mismatch stops before writing. The partition table is read, never rewritten. No erase-all operation is used.
