# Freslest 6.11 BLE keyboard and mouse

Open Settings > Bluetooth. RIGHT turns BLE on; LEFT turns it off and releases
its held keys/buttons. FIRE scans for six seconds. Put the peripheral in its
**Bluetooth LE pairing** mode, select it with UP/DOWN, then OK connects.
OK on a connected device disconnects it. Mouse row selection also works.

If a six-digit code appears, type it on that keyboard and press Enter there.
For a numeric comparison, check both displays and press OK only if they match.
BACK cancels a pending pairing when leaving the page. Unsupported PIN-entry
methods are rejected rather than accepted with an invented PIN.
Select Forget all paired devices, then OK again to remove bonds and disconnect.
LEFT cancels that confirmation. This does not reset Wi-Fi or files.

Two BLE devices may connect at once: for example a keyboard and a mouse, or
a combined device. USB peripherals can remain connected. OS navigation, text
entry, NES controls, mouse pointer and click use the same input state. Disconnect
releases only that device's keys; another keyboard holding the same key keeps it
held. Mouse presence controls pointer visibility.

Arrows/WASD navigate; Enter/Space selects; Escape returns. In NES, X=A, Z=B,
Enter=Start, right Shift=Select, arrows/WASD move. Note accepts the existing US
ASCII layout. Bond keys and the two selected peer addresses/types/names persist
in NVS. After restart or an unexpected disconnect, wake the peripheral:
background scans try to reconnect it automatically. Saved devices are listed
even before a scan. A saved wake advertisement need not repeat the HID UUID.
Selecting an already connected device disconnects it and pauses that peer's
automatic retry until selected again or Bluetooth is turned off/on.

Reconnect attempts use a bounded backoff, with short background scan windows
instead of continuous scanning. A failed connection does not block display
updates. Turning Bluetooth off cancels pending acceptance and releases input;
late SDK callbacks are rejected. The serial diagnostics include disconnect
reason and attempt counts. This improves recovery, but radio interoperability
and the cause of a particular mouse's original disconnection need board tests.

ESP32-S3 provides Bluetooth LE and cannot connect to Classic-only Bluetooth
HID devices. A 2.4 GHz dongle uses the existing USB keyboard/mouse host instead.
See the official [ESP32-S3 features](https://www.espressif.com/en/products/socs/esp32-s3).
BLE support is in OS/NES. The separately booted Doom app keeps its existing
USB keyboard/mouse and physical buttons; BLE is not initialized there.

The input decoder accepts common keyboard arrays, modifier bitmaps, NKRO and
relative mouse reports with signed axes, including 12-bit and 16-bit layouts.
Up to two HID report maps, eight input report IDs per map, 64 fields per map and
64-byte reports are bounded explicitly. Unsupported/vendor layouts are rejected.
Scan discovers advertised HID service UUID 0x1812 or HID appearance; use pairing
mode if the device does not appear. Physical radio/device interoperability has
not been tested in this environment.

Implementation uses ESP-IDF 5.4.4 esp_hid with Bluedroid GATTC and bonded SMP.
GATTS/advertising is replaced by the HID central. Connections run on a worker;
stack callbacks never draw on the display. Off/cancel releases inputs, and
descriptor/length/rollover checks precede state updates. No arbitrary pairing
request or numeric comparison is automatically accepted.
