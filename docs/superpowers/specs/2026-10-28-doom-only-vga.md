# Doom-only VGA repair

The user explicitly requires changes only to Doom's VGA behavior, preserving OS and resolution/settings. Previous complete installers were the wrong delivery scope. A full OS/Doom rebuild and a shared video-component modification are rejected for this change. Use a private Doom component plus a dedicated Doom-partition updater.

Doom's previous VGA branch submits every full 640x480 scanout even when a 320x240 source/menu is identical. A harness using the actual old spi_lcd_send branch reproduced 20 submissions for 20 unchanged frames, where one is needed. Add a Doom-only successful-frame cache with palette, fit and cursor invalidation, retry after dropped submission, and nominal 30 FPS pacing. Keep the 320x240 game render, 640x480 signal, existing fit and 25.175 MHz clock. The private component reads the existing fit setting and keeps output/recovery choices in RAM; it does not save OS display NVS.

Only Doom is compiled. Its CMake checks that the private backend is selected. OS, shared runtime, common components and the full installer are unchanged relative to 4126b0427d08e13d1df8ad00187e29efa3924a3b. The separate updater packages one Doom binary, checks the existing A/B partition layout by reading it, and issues one write to 0x20000, within the 0xe0000 Doom slot. It never writes bootloader, partition table, OTA data, OS, NVS, WAD, filesystem or saves. The regular OS build no longer triggers for Doom-only file changes.

This fixes a confirmed redundant-submission defect and isolates the flash scope. The existing C4 photograph had incorrect intensity steps independently of Doom's palette. No new hardware color conclusion is justified, and the final TV colors/flicker have not been observed here. Do not label the persistent red tint as proven fixed. No on-device test screen is added.

## Delivery

- Successful Doom-only workflow: 40757320257, source 4d617ce1035da6a90ee7dda375836935c78da0fe. The OS-preservation guard passed; no OS or shared runtime changed, and no OS app was built. Main full-installer workflow 40756862544 was skipped.
- One Doom firmware: 888,384 bytes, SHA256 7b19f26722a519b35ae6e0b806dbc712b4381ae3372f1dc684f5c0b745395736. Firmware artifact 12846492180.
- Dedicated Windows updater artifact 12846212568. Native launch/close passed without starting flashing.
- Delivered Freslest-Doom-VGA-Only.exe: 12,609,606 bytes, SHA256 d19cc4eac2361962895682d8f152acb22ef58101ce36a651b9b09971ef4c9835.
- Five targeted tests passed on Linux, including actual frame cache/drop/cursor/palette C paths and Doom backend NVS preservation. Windows flash-boundary tests passed with Linux C checks skipped. Fresh review found no blockers; exact tick-phase timing is nominal rather than a strict measured 30 FPS.
- The first run 40756862706 stopped before builds at container Git ownership; the retry scoped safe.directory to the exact checkout and retained the preservation guard.
- Physical TV color/flicker outcome remains unverified. The persistent red tint is not claimed fixed.
