# Revert the word-scanout regression

The user reports flicker after the 16-bit word output change, with photos still showing abnormal colors. The word output did not solve the reported color failure and doubled scanout PSRAM bandwidth and frame allocation. Revert to native 8-bit RGB332 with the original GPIO order. Preserve 640x480 and the 25.175 MHz target clock.

Increase each bounce buffer from 10 to 20 lines: two buffers now use 25,600 internal bytes, matching the previous word-build internal allocation. Native 8-bit framebuffers return to 614,400 PSRAM bytes total. This halves the scanout payload and removes the backwards expansion pass. The bounce refill deadline doubles and interrupt frequency halves.

Both projects previously disabled CONFIG_LCD_RGB_ISR_IRAM_SAFE. Enable it in sdkconfig and defaults so LCD/GDMA interrupt service remains enabled through flash critical sections; callbacks were already IRAM_ATTR. Explicitly disabling hardware conversion and bit/byte reversal remains. Existing completion-gated frame ownership is retained. No new test screen or menu is added.

The red color cause remains unresolved: the supplied direct C4 image was also abnormal, so source palette corrections and successful host checks cannot establish physical color correctness. Do not claim this bounded flicker regression change solves that color issue. The supplied VIDEO V2.5 FAST SPI STREAM serial log comes from a different LCD/SD player, not the packaged Freslest VGA backend.

## Build and delivery

Source e0f0a5c9d1e305ac9d2cf9ced26a588952561b52; workflow 40712563843 succeeded for six firmware images and native Windows packaging/launches. The staged binaries contain native 8-bit scanout and no word-scanout log. Installer Freslest-Installer-1.1-VGA-DMA.exe: 35,247,677 bytes; SHA256 770949740622c70c6c2665d731a52fe6c1681463aac90adf06393c0a6846dcf9.

The recovered C4 image shows non-monotonic red steps (some higher codes darken while lower codes are bright). This is evidence downstream of the Doom palette. Glare prevents a reliable full electrical diagnosis; an actual close-up of the RGB resistor/GPIO wiring is needed before deciding further color-channel changes. The new physical flicker result has not yet been observed.
