# Freslest AV Engine 0.1.1

VIDEO OUT ladder order: 48 -> 39 -> 40 -> 42 -> 2 -> 14 -> 13 -> 8.
D0..D7 is 8,13,14,2,42,40,39,48.

Reusable ESP-IDF v5.4.4 / ESP32-S3 component for an external 8-bit resistor DAC.
Add this directory as components/freslest_av in an IDF project and declare
REQUIRES freslest_av in the consumer component. Include freslest_av.h.

The full Freslest 6.10 package already integrates it in OS and Doom. To test on
that device, use its full installer and hold physical UP during RESET. The
standalone source archive is a component, not an OS installer.

A minimal diagnostic consumer can call freslest_av_lab_run(NULL) from app_main
with PSRAM enabled on an N16R8 board. It uses physical buttons and prints to
the configured IDF console. See AV_ENGINE.md in the source archive/full project
for original GPIOs, timing, API ownership, tests and hardware limitations.

One rendering task owns acquire/publish/cancel. Never edit an active or pending
field. No per-frame allocation is required. Close the engine before reusing
LCD_CAM. Board/TV compatibility remains unverified by the host tests.
