# Freslest OS 1.0

**LCD/VGA update:** ILI9488 and ILI9341 profiles, native 320×240 menus, and the same LCD RGB565 image converted to physical RGB332 VGA. Composite AV has been removed. See [VGA wiring](VGA_DISPLAY_TR.md) and [release notes](RELEASE_LCD_VGA.md). Firmware and Windows CI results are recorded separately; physical hardware validation remains pending.

**A graphical, desktop-style embedded environment for the ESP32-S3**, featuring applications, NES emulation, DOOM, USB input, file management, and experimental RGB332 VGA.

> **Public version:** Freslest OS **1.0**. `OS/CMakeLists.txt` sets `PROJECT_VER` to `1.0`. Current LCD profile OS and Doom images are rebuilt from this source; the old baseline payload is used for unchanged data assets.

## Firmware source code

The ESP32-S3 firmware sources are included directly in this repository:

| Source | Contents |
|---|---|
| [OS/freslest](OS/freslest) | Freslest OS C++ and Arduino entry point; apps, settings, NES integration, display, audio and input |
| [OS/components](OS/components) | Bundled and modified Arduino, LovyanGFX, TinyUSB, Nofrendo and other dependencies |
| [Doom](Doom) | ESP-IDF Doom project, PrBoom engine and ESP32-S3 platform code |
| [common](common) | USB update implementation shared by the firmware |
| [shared_components](shared_components) | Shared HID, OTA, VGA video and USB host components |
| [installer](installer) / [viewer](viewer) | Python sources for the separate Windows PC applications |

Start with [freslest.ino](OS/freslest/freslest.ino), [apps.cpp](OS/freslest/src/freslest/apps.cpp), or [Doom app_main.c](Doom/main/app_main.c). The board firmware is written in **C and C++**. The installer and viewer are separate PC tools.

### Build the board firmware

Use an **ESP-IDF v5.4.4** command prompt. The modified Arduino core and other component sources are already bundled in `OS/components`; install the ESP32-S3 toolchain through ESP-IDF.

From the repository root:

```powershell
idf.py -C OS build
idf.py -C Doom build
```

The outputs are `OS/build/freslest_os_s3.bin` and `Doom/build/freslest_doom_s3.bin`. Both projects use the checked-in partition layout. Full installation and release-packaging scripts also expect the firmware/data assets from the full release package. WADs, NES game ROMs, recorded MP3s, prebuilt EXEs and ELF debug files are separate from this source checkout.

Host checks that compile production C/C++ modules with SDK test doubles include `python tests/run_screen_input_tests.py`, `python tests/run_power_recovery_tests.py`, `python OS/tests/run_ble_tests.py` and `python OS/tests/run_usb_storage_tests.py`. The full NES and Doom host runners additionally require their game-data fixtures.

## Hardware requirements

- ESP32-S3 N16R8 development board, **16 MB flash / 8 MB OPI PSRAM**
- **ILI9488 480 × 320 SPI TFT** (non-touch configuration)
- Seven momentary buttons, wired **active-low** (between GPIO and GND)
- Optional: DFPlayer Mini and microSD, DHT11, VGA RGB332 resistor DAC, amplifier, single-cell battery with sensing divider, USB host peripherals

## Pin connections

These assignments are based on the provided Freslest ESP32-S3 source archive's hardware table. GPIO means a signal on the ESP32-S3, **not a numbered physical header pin**.

### ILI9488 TFT display

| TFT signal | ESP32-S3 | Notes |
|---|---:|---|
| MOSI / SDI | GPIO11 | SPI data to screen |
| SCK / CLK | GPIO12 | SPI clock |
| CS | GPIO10 | Chip select |
| DC / RS | GPIO9 | Data/command |
| RST / RESET | GPIO8 | **See GPIO8 conflict warning below** |
| LED / backlight control | GPIO41 | PWM to the existing **MOSFET driver**; NOT direct LED supply |
| MISO / SDO | Not used | Leave disconnected |
| TFT touch CS / DO / IRQ | Not used | Touch is disabled |

Supply display power at the voltage specified by **your particular module** and connect grounds. TFT breakout boards differ in whether they tolerate 5 V power and logic; do not assume.

### Physical buttons

| Button | ESP32-S3 | Other terminal |
|---|---:|---|
| UP | GPIO4 | GND |
| DOWN | GPIO5 | GND |
| LEFT | GPIO6 | GND |
| RIGHT | GPIO7 | GND |
| OK / USE | GPIO15 | GND |
| BACK / EXIT | GPIO16 | GND |
| FIRE / SHOOT | GPIO3 | GND |

Buttons are active-low. These are the Freslest-specific seven-button assignments, not a generic ESP32-S3 wiring pattern.

### Audio / DFPlayer Mini

| Connection | ESP32-S3 or destination | Notes |
|---|---|---|
| DFPlayer TX | GPIO17 (ESP RX) | Cross TX to RX |
| DFPlayer RX | GPIO18 (ESP TX) | Source project uses **1 kΩ series resistor** |
| DFPlayer power | Suitable stable supply | Manufacturer typically recommends 5 V; check your board |
| DFPlayer GND | ESP32-S3 GND | Common ground required |
| NES / DOOM live effects output | GPIO21 | To the **existing filter/amplifier circuit**, not a speaker directly |

The DFPlayer handles recorded music/cues while GPIO21 provides live effects. An amplifier and proper interface are required for audio output.

### DHT11, battery monitor, USB

| Signal | ESP32-S3 | Notes |
|---|---:|---|
| DHT11 DATA | GPIO38 | Bare DHT11 requires 4.7 kΩ to 10 kΩ pull-up to 3V3; some modules include it |
| DHT11 VCC | 3V3 | Common GND |
| DHT11 GND | GND | Common GND |
| Battery ADC | GPIO1 | Existing **100 kΩ / 47 kΩ** voltage divider; verify orientation and ADC voltage before wiring |
| Native USB D− | GPIO19 | Prefer built-in USB/OTG connector |
| Native USB D+ | GPIO20 | Prefer built-in USB/OTG connector |

**Never connect a Li-ion battery cell directly to GPIO1**. Use the correctly oriented voltage divider and an appropriate protected battery supply/charging circuit. USB connector roles vary by operating mode.

### VGA RGB332 output

The same LCD RGB565 framebuffer is converted to 640×480 RGB332 (256 colors). VGA uses GPIO41/8/13/14/2/42/40/39 for D0..7 and GPIO47/48 for HSYNC/VSYNC. See [VGA_DISPLAY_TR.md](VGA_DISPLAY_TR.md) for the exact resistor table and shared LCD/SD pin routing. This is a mode-dependent output; the shared pins require hardware isolation. Composite AV is removed. Signal stability remains experimental.

## Installing on Windows

### Windows installer

The **Freslest Installer.exe** workflow:

1. Start the installer and choose **Türkçe** or **English**.
2. Select **ILI9488** or **ILI9341** on **Step 1: Displays**, then choose the COM port.
3. Review the flash/data warning and start installation.
4. Keep the ESP32-S3 connected until flashing completes, then reset it.

The separate **Freslest USB Viewer.exe** launches directly into the viewer interface. Both x64 Windows EXEs were built and passed native launch/close checks; 35 Windows source tests passed. Reports and SHA-256 hashes are included with the Windows download package. Physical ESP32-S3 flashing and USB display/control were not tested in that build environment.

### Developer/manual flashing

The provided project includes `flash_all.bat COM5` and expects the programming/UART COM port. Follow its instructions for required esptool/Python version. For an initial install, the original manual script supports:

```bat
flash_all.bat COM5 --with-files
```

**Warning:** `--with-files` replaces the packaged LittleFS image, including user files/ROMs/notes. Back up important data. The custom partition layout includes separate OS and DOOM images; do not use generic `idf.py flash` to overwrite the wrong slot.

If the port is stuck connecting, use BOOT/RESET download mode as described in the source package. Close other serial monitors or viewers first.

## USB Screen Viewer

The source documentation describes two PC connection modes:

- **USB/UART COM:** viewer connects through the programming/serial port, leaving native USB/OTG for external keyboard, mouse or storage.
- **Native USB CDC:** enable **Settings > Display > USB PC Viewer**, confirm and restart. Native USB may enumerate as a COM port, but in viewer device mode it is **not** simultaneously a USB-host connector.

Use **Fit** to include top and bottom bars. Large frames over serial can be slow; throughput depends on port and connection mode. Viewer compatibility should be checked against the final Freslest OS 1.0 firmware before publishing.

## Included applications and features

- GUI with system status and on-device controls
- NES library/player and other applications
- DOOM and sound support
- Basic file management and USB input support
- Wi-Fi/BLE features and lightweight standalone browser (not a modern full-browser engine)
- Experimental VGA output and recovery/update functions

## Repository layout

`OS/` and `Doom/` are the ESP-IDF projects; `common/` and `shared_components/` contain shared firmware code. `tests/`, `OS/tests/`, `Doom/tests/` and `validation/` contain host checks. `tools/` contains flashing and packaging helpers. Windows sources are in `installer/` and `viewer/`, with their build workflow in `.github/workflows/`. `prepare_firmware.py` reconstructs the preserved payload and applies the verified OS 1.0 metadata stamp for Windows installer builds.

## Important publishing notes

- Include license and attribution for **DOOM port code, libraries and external assets**, respecting each dependency's license. Do not publish copyrighted commercial game ROMs, MP3 files or WAD assets without redistribution rights.
- Publish SHA-256 sums for installer and firmware; test releases on the target board before saying “one-click ready”.
- Future OS source builds use version `1.0`; verify the board behavior before describing a release as hardware-tested.
- Hardware diagrams, battery safety and VGA DAC resistor values must be checked against the finished board-specific wiring before advertising them as fully verified.

## License

License information is **to be confirmed**. Do not claim an open-source license until the code and dependencies have been reviewed and an appropriate license file has been added.
