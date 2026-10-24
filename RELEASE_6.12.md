# Freslest OS 6.12

The Browser no longer needs an external PC or server. It fetches HTTP/HTTPS
directly on ESP32, showing HTML/text, links, basic GET forms and JPEG images.
BrowserServer, remote-control endpoints and server/key setup were removed from
this package and the phone portal.

This is a standalone text browser, with no JavaScript engine or video codec.
The full YouTube/ChatGPT web interfaces and YouTube video are unsupported;
the home links and Info page state the limits. See [BROWSER.md](BROWSER.md).

Wi-Fi setup on device/phone, persistent Wi-Fi outside Settings, BLE reconnect,
automatic Turkey-time NTP and the requested header arrangement remain from
6.11. DHT11 stays on GPIO38. Doom's entire source and firmware are exactly 6.10.
AV Engine 0.1.1 and its DAC order remain unchanged:
48 -> 39 -> 40 -> 42 -> 2 -> 14 -> 13 -> 8 from VIDEO OUT.

Existing A/B users: copy `Freslest-Update-6.12.zip` unextracted to a FAT32 USB
drive and select it in Freslest Update. This replaces only the inactive OS slot.
The complete archive also contains firmware and sources for a first install.
Use the existing UART programming port and `flash_all.bat COM5`, changing COM5
to your port. Ordinary flashing retains ROMs, Note, NVS and saves.

The existing A/B layout and rollback are preserved. The common CA bundle fits
HTTPS into the existing OS slot without moving Doom or user-data partitions.
Software build/parser/worker/UI tests passed; physical board operation is
unverified. Earlier release documents and validation logs are historical.
