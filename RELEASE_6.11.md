# Freslest OS 6.11

This package updates Freslest OS. Doom is retained byte-for-byte from 6.10,
including its firmware and all source/assets. The small update ZIP writes
only the inactive OS slot.

- Persistent BLE peer selection and automatic reconnect after restart or
  unexpected disconnect, with bounded scans/retries and input release.
- Wi-Fi network scan/password entry on the device, plus phone setup portal.
  Leaving Settings keeps Wi-Fi connected; explicit OFF retains credentials.
- Apps > Browser: Google, YouTube, ChatGPT in order, address entry and history.
  A same-network PC/Pi runs Chromium for JavaScript/video and streams bounded
  JPEG frames to ESP32. **The host must stay on; audio plays on the host.**
- Browser mouse/keyboard forwarding, physical scrolling/focus navigation,
  on-screen text entry and server setup. LCD Apps menu scrolls to fit Browser.
- Automatic Turkey-time NTP sync; time and GPIO38 DHT11 temperature beside
  FRESLEST. Wi-Fi and BT indicators immediately left of the battery. Existing
  absent-battery cross retained.

Read [BROWSER.md](BROWSER.md), [WIFI.md](WIFI.md) and
[BLUETOOTH_INPUT.md](BLUETOOTH_INPUT.md) for setup and limitations.

For existing A/B v2 installations, put `Freslest-Update-6.11.zip` unextracted
at the root of a FAT32 USB drive and use Freslest Update. Power the UART port;
attach the drive to USB/OTG with its existing powered OTG connection.
The complete archive includes the PC BrowserServer. A first serial install:

```bat
flash_all.bat COM5
```

Replace COM5 with the programming/UART port; close any Serial Monitor using it.
Ordinary installation retains ROMs, Note, NVS settings and saves. The separate
`--with-files` option intentionally restores the initial LittleFS image.

Retained: A/B v2 partitions and rollback, Files C:/USB D:, USB/NES transfer,
physical buttons, NES, MP3/WAD files, AV Engine 0.1.1 and its existing waveform,
and VIDEO OUT ladder order **48 -> 39 -> 40 -> 42 -> 2 -> 14 -> 13 -> 8**.

Validation includes a real ESP-IDF 5.4.4 OS cross-build, sanitizer-backed host
tests, actual Chromium JavaScript/input/history and H.264 MP4 playback, update
image/ZIP checks and simulated power-loss rollback. Hardware RF, ESP32 Browser
FPS, Windows/Pi operation and physical TV quality have not been measured here.
The remote browser is a low-frame-rate preview, not a promised smooth video
player; normal website login, browser-detection and protected-media limits apply.
