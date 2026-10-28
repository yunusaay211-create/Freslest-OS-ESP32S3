# Revert the word-scanout regression

The user reports flicker after the 16-bit word output change, with photos still showing abnormal colors. The word output did not solve the reported color failure and doubled scanout PSRAM bandwidth and frame allocation. Revert to native 8-bit RGB332 with the original GPIO order. Preserve 640x480 and the 25.175 MHz target clock.

Increase each bounce buffer from 10 to 20 lines: two buffers now use 25,600 internal bytes, matching the previous word-build internal allocation. Native 8-bit framebuffers return to 614,400 PSRAM bytes total. This halves the scanout payload and removes the backwards expansion pass. The bounce refill deadline doubles and interrupt frequency halves.

Both projects previously disabled CONFIG_LCD_RGB_ISR_IRAM_SAFE. Enable it in sdkconfig and defaults so LCD/GDMA interrupt service remains enabled through flash critical sections; callbacks were already IRAM_ATTR. Explicitly disabling hardware conversion and bit/byte reversal remains. Existing completion-gated frame ownership is retained. No new test screen or menu is added.

The red color cause remains unresolved: the supplied direct C4 image was also abnormal, so source palette corrections and successful host checks cannot establish physical color correctness. Do not claim this bounded flicker regression change solves that color issue. The supplied VIDEO V2.5 FAST SPI STREAM serial log comes from a different LCD/SD player, not the packaged Freslest VGA backend.
