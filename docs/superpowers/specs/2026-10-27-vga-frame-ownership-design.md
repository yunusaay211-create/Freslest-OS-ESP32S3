# VGA frame ownership, Doom palette and TV safe area

User reports new physical evidence after the preceding candidate: horizontally multiplied/trembling OS menu text, cropped header/footer, and severe Doom color corruption. They request a dedicated Doom RGB332 path and fixes, within the already authorized build/installer workflow. The earlier generic dark-gray test did not establish the cause of physical red tint.

Transport defect reproduced: submit writes the active scanout buffer. IDF5.4.4 also documents that unconditional VBlank restart may itself produce frame shifts if its interrupt is late. Retaining single direct PSRAM scanout and changing gain/timing again is insufficient. Selected engineering approach: two PSRAM frames, two internal ten-line bounce buffers, explicit frame-complete ownership and bounded waiting. Enable PSRAM XIP to keep cache available during flash/OTA/NVS writes, and stop unconditional per-VBlank restarts; bounce driver can detect short frames and recover. No porch/pin changes or LCD behavior changes.

Doom receives its own raw PLAYPAL RGB888-to-RGB332 lookup and 320x240 indexed renderer, bypassing the LCD RGB565 intermediate and the generic palette conversion. Quantize once per palette change, preserve real damage/pickup palettes, use a fixed ordered pattern rather than temporal alternation, and log palette changes for diagnosis. This is a tested candidate; no physical red-tint cure is asserted without device confirmation.

TV fit adds centered 80/85/90/95/100% safe area, default90%; 640x480 signal remains unchanged. Store fit separately in fl_video so both OS and Doom use it. Display settings offer an explicit size row; LCD and USB pointer coordinates remain source coordinates. Borders black, complete source endpoints visible, no cropping.

Constraints: both LCD models, 16MB A/B layout, existing sound recovery, old AV removal, healthy boot/rollback and USB Viewer retained. Full installer must carry updated Doom; OS update ZIPs cannot replace Doom. Physical timing/TV validation remains outstanding.

Follow-up: user explicitly requests ILI9341 in the change-display menu. Both models are listed and the currently compiled one marked [loaded]. The other selection gives the required installer model; runtime driver replacement is not implemented and no fake setting is saved.
