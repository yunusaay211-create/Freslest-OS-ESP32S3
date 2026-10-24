# Freslest AV Engine 0.1.1 / ESP32-S3

This GPIO-preset update of our experimental CVBS engine is integrated in
Freslest OS and Doom 6.10. It changes only the DAC bit-to-GPIO mapping and
its displayed wiring information from 6.09; timing and rendering are retained. It is cross-built and software-tested. The physical
TV, DAC network, cable, output clock and game FPS have not been measured here.
It is not a claim that the earlier Samsung picture is fixed.

## Install on the existing Freslest device

Extract the full 6.10 ZIP. In its Freslest-OS-Doom-S3 directory run:

```bat
flash_all.bat COM5
```

Use the programming/UART COM port, replacing COM5 if needed. Ordinary flashing
preserves ROMs, Note, NVS settings and Doom saves. Do not add --with-files for a
routine update. The full installer updates both OS and Doom. The compatible v2
USB ZIP updates OS only. On a trial USB update, first let normal OS finish its
startup confirmation before entering the lab.

## First physical test: the static AV Lab

1. Hold the **physical UP button (GPIO4)** and press RESET.
2. Release UP when the test appears. Select AV/composite and 4:3 / Original on
   the TV. The initial scene is a monochrome grid, grey ramp and inset border.
   It deliberately has no colour burst.
3. UP/DOWN cycles: Mono grid, Colour bars, Mono text, White. OK repeats a scene.
4. BACK returns to normal OS after UP and BACK are released.

This one-boot mode does not change display preferences. Holding physical BACK
(GPIO16) during RESET retains priority and forces LCD recovery. USB/Bluetooth
keys are not used to enter this mode.

The lab runs the same raw transport as normal Freslest, before storage, USB,
Bluetooth, sound or game services start. While a scene is stationary there are
no render rewrites; DMA keeps repeating its completed field.

In the UART monitor at 115200 baud, send lowercase commands:

| Command | Result |
|---|---|
| 0 | Monochrome grid and grey ramp, no burst |
| 1 | RGB332 colour bars, burst enabled |
| 2 | Monochrome text and grid, no burst |
| 3 | Solid white active region, no burst |
| i | Field, swap, descriptor error, running-state and timing counters |
| b | Return to normal OS after button release |

For the first board result, report whether scene 0 is stable and readable and
whether scene 1 adds stable colours. The UART `i` output is useful with that
result. Software EOF periods are not an oscilloscope measurement. A missing
picture can still require checking the analog circuit or a transport detail.
The lab does not confirm a pending OTA trial; normal OS performs that check.

## Signal ownership

The engine uses LCD_CAM in raw i80 data mode, with a circular GDMA descriptor
stream. It owns CVBS sample timing, sync/burst generation, rendering and field
handover. ESP-IDF HAL/GDMA provides the hardware access. No RGB panel driver is
used for composite output. Every regular line has exactly 910 stored samples,
including sync, back porch, active video and front porch. There are no separate
hardware RGB porch clocks between those samples. One startup dummy cycle primes
the peripheral before the continuous stream; it is not repeated per field.

The field is 262 lines / 238420 bytes. It is a 240p-style progressive NTSC-family
signal, not a 525-line interlaced broadcast encoder. The ideal sample rate is
14.318181818 MHz, colour carrier 3.579545455 MHz, horizontal 15.734266 kHz and
progressive refresh 60.054449 Hz. The requested sample clock is 14318181 Hz;
the IDF fractional divider predicts PLL240M / (8 + 24/63) / 2. That prediction
retains fractional-clock jitter; no external crystal or physical measurement
is added by software.

| Region | Samples / placement |
|---|---|
| Normal H sync | 67 low samples at line start |
| Pre-equalization | Lines 0..2: 33 low samples per 455-sample half-line |
| Broad sync / serration | Lines 3..5: 388 low and 67 blank per half-line |
| Post-equalization | Lines 6..8: 33 low samples per half-line |
| Colour burst | Normal lines 9..261, samples 75..114, optional |
| Image | Lines 20..259, samples 136..879, 320x240 canvas |
| DAC codes | Sync 0, blank 73, black 86, white 255; burst +/-36 |

Burst and active chroma follow absolute sample phase, including the alternating
180-degree line phase. Pre/post-equalization and serrations have no burst.
Active pixels use byte stores because alternate 910-byte lines are not 32-bit
aligned. This avoids an alignment fault on the ESP32-S3.

## Completed fields, not live waveform edits

Three whole-field wave buffers live in aligned PSRAM. GDMA descriptors live in
internal RAM and split each field into 60 nodes: 59x4032 bytes plus 532 bytes.
Allocation is rounded to 238464 bytes; the extra padding is never transmitted.
The DMA strategy keeps owners unchanged, disables owner checking for the ring,
and reports EOF after data is popped from the DMA FIFO. Descriptor EOF marks a
field event and does not terminate its circular link.

The renderer writes only a reserved free field and flushes it completely before
publication. The old ring's final next pointer is linked to the completed new
ring. A late hardware prefetch may repeat one old complete field. The old field
is not reusable until the NEW field's EOF is observed. A third field can be
prepared while that handover is pending. The ISR does not render or copy PSRAM.
There is no per-field stop/start and no partial waveform update in the active
ring. Descriptor errors, unknown EOF addresses, stopped LCD state and missing
field progress fail the engine health check.

The Freslest adapter retains its separate owned 320x240 canvas queue. Normal
LCD rendering, user controls, files/audio and the A/B layout keep their existing
paths. Startup health requires a completed handover and at least three field
events; a rendered or merely queued first frame does not count as displayed.

## Rendering and controls

RGB565 or indexed OS/Doom images are converted to the existing RGB332 canvas.
The engine builds four palette phases with neutral greys and chroma-vector
headroom limiting. Horizontal interpolation blends each unmodulated phase.
Sharp selects interpolated samples directly. Clean adds a four-sample phase-lane
carrier trap. Packed two-lane arithmetic and rolling sums avoid per-tap floating
point and per-frame allocation. These are new rendering paths; Sharp is not the
old 6.08 renderer. Color 0 disables active chroma and burst. Hue/Color/Detail
save/cancel behavior and v1-to-v2 settings compatibility are retained.

Settings > Display > TV alignment still controls image size and position;
the UI stays 480x360 -> 320x240 uniformly. LCD stays 480x320. A/B firmware updates,
watchdogs and rollback remain enabled.

## Wiring for 6.10

| Bit | GPIO |
|---|---|
| D0 | 8 |
| D1 | 13 |
| D2 | 14 |
| D3 | 2 |
| D4 | 42 |
| D5 | 40 |
| D6 | 39 |
| D7 | 48 |

Use the existing resistor DAC and RCA connection with the following pin order.
Physical ladder order from
VIDEO OUT is 48,39,40,42,2,14,13,8. LCD remains off/CS high in composite mode.
Buttons, DHT11 GPIO38, DFPlayer RX17/TX18, SFX GPIO21 and native USB19/20 retain
wiring. The new driver does not route a separate physical PCLK/HSYNC/VSYNC pin.

## Source and reproducibility

The reusable component is `shared_components/freslest_av`:

- av_signal: all physical samples, palette, interpolation and diagnostic scenes.
- av_ownership: explicit active/pending/writing field ownership.
- av_engine: ESP32-S3 LCD_CAM/GDMA/clock/resource management and health.
- av_lab: one-boot static tests and physical/UART controls.

The `freslest_video` component adapts existing OS/Doom producers and settings.
Legacy waveform routines remain for regression comparison; production composite
output calls the new component. IDF v5.4.4 is required because the component
uses its private GDMA/LCD common interfaces. The engine code is MIT licensed.
Use `python tools/build_all.py` from an ESP-IDF v5.4.4 command prompt to rebuild
the complete project, then the ordinary flasher above.

Host tests cover all field samples, sync intervals, a scalar interpolation/trap
oracle, palette/flat-colour phase, unaligned buffer guards, 20000 ownership
handovers, actual driver calls against a hardware mock, delayed descriptor
prefetch, all fallible initialization boundaries, and existing application and
update checks. A mock cannot prove electrical timing, PSRAM arbitration under
flash writes, peripheral interoperability or TV lock. BUILD_REPORT.json records
results and image limits. Physical testing is the next validation step.

Primary references: [Espressif LCD i80 HAL](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-reference/peripherals/lcd/i80_lcd.html),
[Espressif i8080 continuous output](https://docs.espressif.com/projects/rust/esp-hal/1.0.0-beta.0/esp32s3/src/esp_hal/lcd_cam/lcd/i8080.rs.html),
[TI LMH1981 vertical interval figures](https://www.ti.com/lit/ds/symlink/lmh1981.pdf),
and [TI composite sample-count table](https://www.ti.com/lit/an/snla114a/snla114a.pdf).
The public [HUB75 DMA driver](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/blob/master/src/platforms/esp32s3/gdma_lcd_parallel16.cpp)
was consulted for continuous LCD_CAM ring behavior. Its source is not bundled.
