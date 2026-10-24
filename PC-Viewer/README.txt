# Freslest OS 6.16 / USB Screen Viewer 2.1

## 2.1 PC performance update

Works with the existing Freslest OS 6.16 firmware. Close Viewer 2.0, extract
this ZIP, and run start_viewer.bat. No firmware reflash is required.
Resolution now defaults to Half (one quarter as many pixels); it keeps the
whole screen including both bars. Full remains selectable before connecting.
Fit zoom and PC keyboard/mouse control remain available on native USB and UART.

The GUI presents at most one newest frame per timer turn, reuses its Tk image
and scaling buffers, and keeps connection events separate from frames. RGB332
conversion uses bounded buffers instead of a large per-pixel join. Serial reads
use available bytes, and disconnect no longer waits for an unbounded driver
flush. UART bandwidth still limits detailed screens: at 921600 baud a worst-case
Full composite frame needs about 1.89 seconds on the wire, Half about 0.47 sec.
Native USB avoids that UART baud bottleneck; Windows can still name it COM.

Measurements on the build machine (not a measured ESP32/Windows session):
480x360 conversion: 15,786,201 -> 1,036,953 peak Python bytes; 5.90 -> 0.45 ms.
Actual Tk 3x-zoom frame-burst test: longest timer gap 1161.82 -> 27.29 ms.
These are stage measurements, not an end-to-end FPS guarantee. Physical Windows
USB and the ESP board were not available. Frame checksums remain enabled.

## Setup

Install OS 6.16 using the update ZIP on an existing Freslest A/B installation,
or the full package's flash_all.bat with the programming COM port for initial
setup. Close Serial Monitor before the viewer or flasher.

Run usb_screen.bat in the full package, or start_viewer.bat in the standalone
Viewer ZIP. Python 3.9+ with Tcl/Tk is required. The launcher installs pyserial
3.5 if missing (Internet needed once). Auto detects supported USB serial ports,
checks the Freslest signature, prefers native USB and reconnects after removal.
A manual port is available for bridges Auto does not recognize.

| Connection | Device setting | PC cable | Physical USB keyboard/mouse |
| --- | --- | --- | --- |
| Programming USB/UART | USB PC Viewer OFF (default) | Programming port | Available on native USB/OTG |
| Native USB CDC | Settings > Display > USB PC Viewer ON, OK, OK to restart | Native USB/OTG port | Use PC input, BLE or onboard buttons |

Native USB CDC also appears as COM on Windows. It uses the ESP USB controller
directly; the viewer identifies NATIVE or UART from the firmware signature.
The baud selector changes only UART. Auto cannot change the board's USB role:
enable the native setting before expecting the native viewer port to enumerate.
One viewer owns the stream. The other port cannot stop or control its session.

The native controller has one active role. To attach a physical keyboard,
mouse or USB drive again, turn USB PC Viewer OFF and confirm restart, or use
Files > Disks > FIRE to switch to host. UART viewing works alongside the USB
host. NES USB transfer takes priority and clears the native viewer setting;
re-enable it afterwards if wanted. The updater checks its boot request before
starting the normal native viewer, preserving USB flash-drive updates.

Fit is the default zoom and shows the complete frame, including both bars.
Manual 1x/2x/3x zoom has scrollbars. Full is 480x320 LCD or 480x360 composite UI;
Half is 240x160 or 240x180 and is now the default. Resolution/baud apply on the next connection. Save
PNG saves the received frame, including pixels outside a zoomed viewport.

Enable PC keyboard + mouse and click inside the image. The canvas must have
focus for keyboard/mouse input. The host cursor hides there, leaving the device
pointer visible. Letterbox borders are ignored. Clicking another control or
losing focus releases PC keys/buttons and restores the host cursor.

- Arrows/WASD: direction. Enter: OK. Escape: BACK. Ctrl or F: FIRE.
- Tab: Task Switcher. Browser text entry reserves Tab for the page.
- NES: X=A, Z=B, Enter=Start, Right Shift=Select, arrows/WASD=direction.
- Standard US keyboard usages are forwarded for supported text input.

Focus loss, disabling input and disconnect release PC input. The ESP also
releases it after 900 ms without a heartbeat. PC input uses its own OS merger,
so both BLE peers and physical USB inputs remain independent. Turning Bluetooth
off does not clear PC input, and PC release does not disconnect BLE devices.

ESP frame and encoded transfer buffers use PSRAM. USB/UART drivers, DMA,
controller allocations and normal task stacks can still require internal RAM.
Wi-Fi/lwIP and Bluetooth retain PSRAM-first allocations and 22,880 bytes of
linked static radio BSS in external RAM. The PC uses its own computer's RAM.

The viewer receives the actual clean digital OS framebuffer and pointer, not
analogue AV noise, TV overscan or sync signals. RGB332 quantizes to 256 colors;
RLE is lossless after that conversion and packets carry a checksum. Detailed
Full UART frames can be below 1 fps; this is not a 60 fps game capture. Try Half
or 460800 baud with an unreliable UART bridge.

OS apps and NES are supported. Boot Recovery works through the UART viewer
without enabling radios or native USB; native starts after normal Continue.
Doom, AV Lab and the separate USB updater do not use the OS mirror loop. Doom
code and binary remain the exact 6.10 versions. This is Python source plus a
BAT launcher, not an EXE. Linux/macOS: install requirements.txt with pip and run
python3 freslest_viewer.py. Linux needs Tk and serial-device access permission.

Viewer 2.1 validation: lossless RGB conversion and peak-memory regression;
real Tk newest-frame, image reuse and disconnect-state tests; serial latency
and stalled-drain regressions; original C++ encoder/Python decoder fixtures,
real pyserial pseudo-terminal, Auto/native/UART/control and Fit/focus tests.
The OS 6.16 firmware/build validation remains the one in the full release;
this client update does not alter its firmware or Doom.

Developer checks from this folder:
python tests/test_performance.py .
python tests/test_transport_latency.py .
python tests/test_gui_performance.py .
The GUI test needs a display; Linux CI can use Xvfb.

## Protocol

At 115200, FS? returns FSHELLO 2 UART|NATIVE <width> <height>.
FS1 <921600|460800> <1|2> starts a session. FSREADY <baud> precedes the UART
baud change; native CDC does not change transport rate. FS2 requests one frame
and refreshes a five-second timeout. FS0 waits for pending TX, ends the session
and restores UART 115200. Other-port start requests get FSBUSY.

FSK <64 hex digits> carries a 256-bit USB HID keyboard bitmap.
FSP <x> <y> <buttons> sets the absolute full-UI pointer (1=left,2=right,4=middle).
FSR releases PC input only. Commands end with newline. The PC refreshes input
every 250 ms and at most 50 times per second. A packet is immutable until the
writer finishes; stop/disconnect frees it only afterwards.

Frames retain protocol 1: 24-byte little-endian <8sIIHHI>, magic FSCRN1\r\n,
payload length, sequence, width, height and FNV-1a32(payload). PackBits tokens
with bit7 repeat the following byte (token&127)+1 times; otherwise copy that
many literal bytes. RGB332 uses red7..5, green4..2, blue1..0.
