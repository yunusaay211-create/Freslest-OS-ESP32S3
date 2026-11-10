# Freslest OS 1.5

Freslest OS for **ESP32-S3 N16R8** includes a graphical OS, NES and Doom, a file manager, USB and BLE input, and three display profiles. VGA opens Freslest Desktop, and the SPI LCD profiles use the classic Freslest interface. All device menus are in English. [Türkçe README](README_TR.md)

## What's new in 1.5

- **Freslest Desktop.** A 640×480 XP-inspired shell with a Start menu, taskbar and draggable windows. Files uses an Explorer-style task pane and Settings opens as a Control Panel. Calculator, Notepad, Paint, Clock and Browser retain their existing tool layouts.
- **FreStore.** Install signed cards and their data to internal flash, SD card or a USB drive. The built-in starter catalog works over HTTPS. Add a personal Raspberry Pi HTTPS server from **Server → Add HTTPS server**.
- **Bluetooth.** BLE stores HID maps and bond-list buffers in PSRAM and checks the remaining internal RAM before pairing. It waits for 2,500 KiB of free PSRAM at startup and reconnects to remembered keyboards and mice.
- **Doom saves.** The Doom launcher exports verified `.dsg` saves to an SD card, USB drive or a PC over native USB. The original saves stay available in Doom.
- **Wi-Fi Update.** Check for a newer signed OS image from the configured Pi. An optional Pi on a second routed network can mirror the signed release with `mirror-os`. After confirmation, the device verifies the image and writes it to the inactive OS slot for a trial boot.
- **Display and input.** VGA frame submission handles delayed DMA without blocking the shell. USB Boot HID releases held keyboard and mouse input on disconnect.

## Downloads

| File | Contents |
| --- | --- |
| [Freslest 1.5 test package](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/42507549212/artifacts/13628262463) | USB update ZIPs, ARM64 and ARMv7 FreStore binaries, SHA-256 sums and test report |
| [Windows x64 installer and USB Viewer](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/42507549212/artifacts/13628815714) | Freslest Installer.exe and Freslest USB Viewer.exe |
| [Build and test run](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/42507549212) | ESP-IDF firmware builds, Go tests, and Windows build and launch checks |

## Firmware source code

The ESP32-S3 firmware sources are included directly in this repository:

| Source | Contents |
|---|---|
| [OS/freslest](OS/freslest) | Freslest OS C++ and Arduino entry point, apps, settings, NES integration, display, audio and input |
| [OS/components](OS/components) | Bundled and modified Arduino, LovyanGFX, TinyUSB, Nofrendo and other dependencies |
| [Doom](Doom) | ESP-IDF Doom project, PrBoom engine and ESP32-S3 platform code |
| [common](common) | Update package validation and flash writer |
| [frestore-server](frestore-server) | Go HTTPS server, package signing and OS update mirroring |
| [shared_components](shared_components) | Shared HID, OTA, VGA video and USB host components |
| [installer](installer) / [viewer](viewer) | Python sources for the separate Windows PC applications |

Start with [freslest.ino](OS/freslest/freslest.ino), [apps.cpp](OS/freslest/src/freslest/apps.cpp), or [Doom app_main.c](Doom/main/app_main.c). The board firmware is written in **C and C++**. The installer and viewer are separate PC tools.

### Build the board firmware

Use **ESP-IDF v5.4.4** and run this command from the repository root.

```sh
python tools/build_lcd_profiles.py
```

The builder compiles OS and Doom for ILI9488, ILI9341 and VGA, checks image sizes and profiles, and stages verified installer files under `firmware/`. `OS/CMakeLists.txt` sets the OS version to **1.5**. `prepare_firmware.py` restores the preserved release data used by the installer.

Desktop, VGA, LCD, USB input and OTA host checks live in `tests/`, `OS/tests/` and `Doom/tests/`. The full NES and Doom host runners require their game-data fixtures. Physical display, USB accessories and the two-router network require target-device testing.

## Hardware requirements

- ESP32-S3 N16R8 development board, **16 MB flash / 8 MB OPI PSRAM**
- **ILI9488 480×320**, **ILI9341 320×240** SPI TFT, or an RGB332 VGA setup
- Seven momentary buttons, wired **active-low** (between GPIO and GND)
- Optional: DFPlayer Mini and microSD, DHT11, VGA RGB332 resistor DAC, amplifier, single-cell battery with sensing divider, USB host peripherals

## Pin connections

These assignments use ESP32-S3 GPIO signal names.

### ILI9488 TFT display

| TFT signal | ESP32-S3 | Notes |
|---|---:|---|
| MOSI / SDI | GPIO11 | SPI data to screen |
| SCK / CLK | GPIO12 | SPI clock |
| CS | GPIO10 | Chip select |
| DC / RS | GPIO9 | Data/command |
| RST / RESET | GPIO8 | Shared with VGA D1 |
| LED / backlight control | GPIO41 | PWM to the existing MOSFET driver. Shared with VGA D0. |
| MISO / SDO | Not used | Leave disconnected |
| TFT touch CS / DO / IRQ | Not used | Touch is disabled |

Supply display power at the voltage specified by **your particular module** and connect grounds. Use the power and logic voltages specified for the breakout board.

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

Buttons are active-low. These seven assignments are Freslest-specific.

### Audio / DFPlayer Mini

| Connection | ESP32-S3 or destination | Notes |
|---|---|---|
| DFPlayer TX | GPIO17 (ESP RX) | Cross TX to RX |
| DFPlayer RX | GPIO18 (ESP TX) | Existing series resistor is **1 kΩ** |
| DFPlayer power | Suitable stable supply | Check the module's supply rating |
| DFPlayer GND | ESP32-S3 GND | Common ground required |
| NES / DOOM live effects output | GPIO21 | To the existing filter and amplifier circuit |

The DFPlayer handles recorded music/cues while GPIO21 provides live effects. An amplifier and proper interface are required for audio output.

### DHT11, battery monitor, USB

| Signal | ESP32-S3 | Notes |
|---|---:|---|
| DHT11 DATA | GPIO38 | Use the existing 3V3 pull-up or the module's built-in pull-up |
| DHT11 VCC | 3V3 | Common GND |
| DHT11 GND | GND | Common GND |
| Battery ADC | GPIO1 | Existing **100 kΩ / 47 kΩ** divider. Verify orientation and ADC voltage before wiring. |
| Native USB D− | GPIO19 | Prefer built-in USB/OTG connector |
| Native USB D+ | GPIO20 | Prefer built-in USB/OTG connector |

Connect the battery sense input through the correctly oriented voltage divider and use an appropriate protected battery supply and charging circuit. USB connector roles vary by operating mode.

### VGA RGB332 and shared pins

VGA targets 640×480 at 59.94 Hz and a 25.175 MHz pixel clock. Freslest Desktop uses a 640×480 canvas. VGA uses GPIO41/8/13/14/2/42/40/39 for D0–D7 and GPIO47/48 for HSYNC/VSYNC. Isolate shared LCD and SD lines in hardware as required by the wiring table. [VGA wiring and resistors](VGA_DISPLAY_TR.md)

### microSD

Insert a FAT16/FAT32 microSD before powering on to install from FreStore or export Doom saves. FreStore also reads earlier packages saved on SD-SPI or USB.

| SD-SPI signal | ESP32-S3 | Notes |
| --- | --- | --- |
| MOSI / DI | GPIO11 | Shared with TFT MOSI |
| SCK / CLK | GPIO12 | Shared with TFT SCK |
| MISO / DO | GPIO46 | Boot strapping pin |
| CS | GPIO0 | Open drain. High at reset for normal flash boot |

An SD module that pulls GPIO46 high during reset can block ROM download mode. [ESP32-S3 boot mode](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html) · [SD pull-up requirements](https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32s3/api-reference/peripherals/sd_pullup_requirements.html)

## Install and update

### Windows installer

1. Open **Freslest Installer.exe** and choose **Türkçe** or **English**.
2. Select **ILI9488**, **ILI9341** or **VGA**, then choose the ESP32-S3 COM port.
3. Review the flash warning and keep the board connected until installation finishes.

Back up user files, ROMs and notes before initial installation. The Windows x64 installer and viewer passed launch and close checks. [Windows build](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/42507549212)

### USB update

For an existing Freslest A/B v2 installation, copy the matching ZIP from `updates/` to the root of a FAT16/FAT32 USB drive. Keep that ZIP intact.

| Installed firmware profile | OS Update |
| --- | --- |
| ILI9488 | `UPD88.ZIP` |
| ILI9341 | `UPD41.ZIP` |
| Dedicated VGA | `UPDVGA.ZIP` |

Use **OS Update** first and wait for a healthy boot. An LCD build switched to VGA in Settings still uses its installed LCD firmware profile for OS Update. To update Doom, choose **Doom Update** and use the same LCD ZIP in LCD mode or `DOOMVGA.ZIP` in VGA mode. [USB update format and steps](UPDATE_FORMAT.md)

### Wi-Fi update and Raspberry Pi

Install this 1.5 build once using USB or the installer. Configure the trusted FreStore server and publisher key. Then open **Start → Wi-Fi Update** on Desktop or **Apps → Wi-Fi Update** on LCD. Review the signed version and publisher fingerprint before selecting **Install and restart**. Routed HTTPS access between the Pi and the device is required. [Pi setup, publish and mirror commands](docs/FRESLEST_1_5_TR.md)

## USB Screen Viewer

- **USB/UART COM.** Connect the viewer through the programming port to keep native USB/OTG available for host peripherals.
- **Native USB CDC.** Enable **Settings → Display → USB PC Viewer**, confirm and restart. This dedicates native USB to the viewer device role.

Use **Fit** to show the whole screen. Frame throughput depends on the port and connection mode. [USB connection modes](NATIVE_USB.md)

## Publishing and license

Include attribution for Doom port code, bundled libraries and third-party assets. Verify redistribution rights for any ROM, WAD and audio files before publishing. Share SHA-256 sums and record physical board tests separately from CI results.

Project licensing is pending review of the code and bundled dependencies.
