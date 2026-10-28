# Explicit VGA word scanout

The user supplied a new red Doom photo after the clock rollback and requested a code fix without another diagnostic screen. The existing C4 photograph also shows incorrect intensity steps, so palette-only changes cannot establish a complete fix. The exact physical cause remains unproven. This change replaces the shared 8-bit scanout path with explicit 16-bit LCD_CAM words, while preserving the existing eight GPIO connections and physical RGB332 DAC.

Canonical RGB332 bytes are expanded backwards in each unused PSRAM scanout allocation to RGB565 word bits B=3,4; G=8,9,10; R=13,14,15. Those bits connect to GPIO41,8,13,14,2,42,40,39 respectively. Every other data output is disconnected. Palette/gamma, scaling and physical DAC weights stay unchanged. LCD_CAM conversion, bit reversal, byte swap and swizzle are explicitly disabled before activation. Both OS and Doom use this backend.

The two scanout buffers now consume 1,228,800 PSRAM bytes and two bounce buffers consume 25,600 bytes internally. Compact conversion uses the lower half of an unowned scanout allocation; backwards expansion does not overwrite unread source bytes. Completion-gated ownership is retained. Resolution remains 640x480, target clock 25.175 MHz, with the existing sync/porches and saved fit.

No new on-device test screen or verification menu is added. Existing host transport checks are adapted to observe the bytes on the eight connected GPIOs from 16-bit words. Physical color correction still requires the actual device; do not claim photographs demonstrate success before a new result exists.
