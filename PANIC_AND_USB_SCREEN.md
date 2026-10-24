# Freslest OS 6.14: Bluetooth panic and USB Screen

The submitted 6.13 crash is inside Bluetooth startup, not a decoded Wi-Fi assertion.
The previous `wifi:m f null` warning is consistent with memory pressure but does
not identify the exact allocation that failed.

The recovered 6.13 sources were rebuilt using ESP-IDF 5.4.4 and GCC 14.2.0
esp-20260121. All executable image segments match the shipped 6.13 image
byte-for-byte; read-only strings differ with build paths/time. This makes its
code addresses suitable for decoding this supplied trace. The exact trace is
saved in `validation/panic-613-decoded.txt`.

The call chain is BTU_StartUp -> error_exit -> BTU_ShutDown ->
btu_task_shut_down -> btu_free_core -> l2c_free -> l2c_free_p_lcb_pool ->
list_begin(NULL) -> assert. At this point BTU's initial startup work has not
initialized the L2CAP list. The compiler shares the cold assert code labelled
list_is_empty; its caller in this trace is list_begin.

Any of BTU_StartUp's three hash map allocations, worker creation, or first
work post can enter this error path. The trace alone cannot identify which.
FreeRTOS task stacks/queues still require internal RAM even with 8 MB PSRAM.

6.14 changes:

- Small eligible malloc allocations above 1024 bytes may use PSRAM (was 16384).
  The internal-only reserve is 65536 bytes (was 32768).
- Arduino UI stack is 24576 bytes (was 32768); compiler stack-usage output was
  audited. This is static stack analysis, not a measured runtime high-water mark.
- Files USB MSC storage service starts on first opening Files. USB HID still
  starts normally; the updater and NES USB transfer retain their own roles.
- Bluetooth checks 64 KiB free internal RAM and a 12 KiB block before starting,
  then checks 32 KiB/8 KiB after controller/BTC initialization before enabling
  BTU. Low headroom reports an error and keeps application BLE disabled.
- Unused BLE peripheral advertising and direct-test-mode commands are disabled
  to keep the UART viewer inside the existing A/B flash slots. BLE scanning,
  HID central connections, saved peers and reconnect are retained.
- The user-facing esptool install instruction uses available release 4.11.0.

These changes reduce pressure and reject known-low headroom before the unsafe
SDK path. Other threads can allocate concurrently; this is not proof that all
possible SDK allocation failures are safe. No SDK assert is disabled. Physical
Wi-Fi/BLE boot, mouse reconnection and USB electrical operation require board
verification. After installation, UART `i` prints radio diagnostics. If another
panic occurs, capture its entire log and decode with the matching 6.14 ELF.

## USB screen viewer

Run `usb_screen.bat` in the full package or `start_viewer.bat` in the separate
viewer ZIP. Python 3.9+ with Tk and pyserial 3.5 is required. The BAT supports
`py -3` or `python`, installing pyserial on first run if needed.

Connect the board's programming/UART USB port (COM5 if still assigned that
number), close Serial Monitor, select the port and Connect. Native USB/OTG is
free for peripherals. Disconnect and wait up to five seconds before logging
at 115200 or flashing.

Supported: normal OS/apps, NES, Recovery and See Log. Not supported: separate
unchanged Doom firmware, AV Lab. The updater owns its own loop, so reliable
continuous capture during update is not promised. View-only: physical, USB or
BLE controls still operate the device. Auto sleep is blocked while mirroring.

Frames are full 480x320 LCD / 480x360 composite UI, or half each dimension,
quantized to RGB332 and RLE compressed. The cursor is composited into the PC
snapshot without modifying the actual UI buffer. A low-priority UART writer
owns one immutable PSRAM packet; the UI never overwrites/frees it during TX.
Every request returns a checksum-checked packet. This is a digital screen copy;
it cannot show or repair analogue composite TV artifacts.

Detailed moving full screens can be below 1 fps at UART speeds. Menus compress
better. Use Half for faster updates or 460800 baud for an unreliable cable.
No external PC rendering server is required; the viewer only displays pixels.

`debug/freslest_os_s3-6.14.elf` and `.map` match the included firmware. With the
ESP-IDF toolchain available, use `python tools/decode_panic.py <addresses...>`.
The helper never interprets a 6.13 address with the 6.14 ELF.

## 6.15 clarification

6.14 already enabled external BSS and PSRAM-first Wi-Fi/lwIP/BT heap routes.
6.15 adds separate INTERNAL/PSRAM diagnostic counters and prints linked radio
BSS placement. It does not move all radio memory or change the allocation policy.
The latest debug ELF/map are named 6.15; see RADIO_PSRAM.md.
