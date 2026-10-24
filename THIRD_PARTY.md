# Third-party source provenance

The PrBoom/ESP32 Doom project and modified dependency sources are included here.
Game ROMs, WADs and recorded audio are runtime data supplied separately from this
source checkout. Historical notes below describe the complete release package.
Local source changes and current behavior are described in README.md. Original
copyright/license notices remain in their source files and component folders.

| Component | Upstream | Revision | License |
|---|---|---|---|
| Arduino ESP32 3.2.1 | https://github.com/espressif/arduino-esp32 | ac961f671abd5ae1da0a15fd4bee71ed807c2cf3 | LGPL-2.1 (LICENSE.md) |
| LovyanGFX | https://github.com/lovyan03/LovyanGFX | 777b345dfffcef83e2e24cc12e4d4fd9eda36e93 | FreeBSD (license.txt) |
| TJpg_Decoder | https://github.com/Bodmer/TJpg_Decoder | 71bfc2607b6963ee3334ff6f601345c5d2b7a8da | Upstream license retained |
| esp_littlefs | https://github.com/joltwallet/esp_littlefs | 75e4cd412d17c18cd8a10d3dbf09d2620b6dd2e8 | Upstream LICENSE retained, littlefs BSD-3-Clause |
| Arduino Nofrendo | https://github.com/moononournation/arduino-nofrendo | 7506ab5be2416357c279802e33d9d6e7a0b92c7e | LGPL-2.0 (COPYING.LGPL) |

Local changes to Arduino: esp_timer, esp_netif, esp_wifi and lwip are public CMake
requirements because Arduino public headers include their headers in IDF builds.
Unused Arduino libraries are disabled in sdkconfig; legacy BLE 4.2 advertising is
enabled for the existing Settings page.

Nofrendo changes: cooperative one-frame API; ESP32 display/input/memory adapter;
8-pixel PPU overdraw allocation; PSRAM bitmap storage; uintptr_t bitmap alignment;
mem_alloc declaration; non-debug allocator compatibility for ROM memory; bounded,
deduplicated input registration/reset; unsigned-char ctype calls for IDF/newlib.
The full modified core sources and its license are included to permit rebuilding
and replacement. No upstream sample firmware display/audio drivers are used.

The eight OS MP3 cues 0105–0112 were synthesized specifically for this update.
Original 0001–0104 MP3 and ADVERT files are byte-for-byte unchanged.

NES sound follow-up: the cooperative Nofrendo frame API now submits the APU's
signed 16-bit PCM to the OS adapter, including skipped video frames. A fractional
sample counter preserves exactly 8000 samples per second at the 60 Hz emulated
frame rate. The new OS PWM backend uses an ESP timer on CPU0 and LEDC channel 4 /
timer 2 with the same XTAL source as Arduino backlight. This avoids reconfiguring
backlight channel 1 / timer 0. A bounded 2048-sample queue, prefill, mute/gain ramp,
underrun fade and generation-checked shutdown handle output lifecycle.


USB Transfer follow-up:
- Arduino TinyUSB component: espressif/esp32-arduino-lib-builder revision
  d09c8c64a862a4cfff4da574be207bf801fd5930; upstream source notices retained.
- TinyUSB 0.18.0: https://github.com/hathach/tinyusb revision
  86ad6e56c1700e85f1c5678607a762cfe3aa2f47; MIT, see bundled tinyusb/LICENSE.
- Arduino USB.h explicitly includes WString.h so USB MSC builds correctly
  with CDC disabled. Only MSC is enabled; UART0 remains the serial console.
- ROM transfer uses a bounded FAT12/FAT16 reader and RAM disk; persistent
  files remain on LittleFS. FAT metadata is not written over the live ROM store.

5.0.0: native IDF GAP preserves FreslestOS advertising and Bluetooth ON/OFF.
USB Host MSC is now in shared_components/usb_host_msc, retaining the pinned
1.3.0 source revision and license. Local changes fix false-success mount returns,
retry readiness, clear drive geometry and add CRC-checked read-only GPT mapping.
The original WAD and all MP3/ADVERT files are retained byte-for-byte. A separate
flash WAD preserves all non-music payloads; MUS data becomes unique identity
tokens for the existing DFPlayer MP3 backend. Host bootloader tests extract
unmodified functions from the selected IDF checkout into temporary build files.


Freslest 5.0.1 local USB changes: the bundled Arduino TinyUSB worker yields,
uses a checked 8 KiB task on core 0 and safely retries descriptor/driver setup.
The bundled TinyUSB device dispatcher has a configurable 16-event run budget.
Original notices and licenses are retained in both source trees.

Freslest OS 6.00 adds eight original synthesized UI MP3s (0105-0112).
See OS_SOUND_LICENSE.txt and tools/make_os_sounds.py. Doom audio is unchanged.
