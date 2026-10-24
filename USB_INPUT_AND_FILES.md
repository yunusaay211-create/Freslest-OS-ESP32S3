# Freslest ESP32-S3 6.12: USB input and Files

## Connection

Power the board through the programming/UART connector. Connect a compatible
keyboard or mouse to the native USB/OTG connector (GPIO19 D-, GPIO20 D+),
using a USB-C OTG connection. Keyboard + mouse together need one USB hub,
or a compatible single composite receiver. The programming connector is not
another USB host. Downstream devices need 5 V VBUS; use a properly powered
OTG hub if your board cannot supply it. No GPIO power-switch wiring is assumed.
Do not connect the PC and input-device hub to the native port at the same time.

Supported: USB boot keyboards and boot mice (three-byte mouse reports, optional
wheel byte ignored), four input interfaces, one hub. Report-only/NKRO and
proprietary gaming interfaces are not supported; select BIOS/6KRO mode if offered.
BLE pairing/reconnect is described in BLUETOOTH_INPUT.md. Physical buttons
remain available if an input device cannot enumerate.

## OS controls

| Input | Action |
|---|---|
| Arrows / WASD | Move through menus; held directions repeat |
| Enter / Space | OK / select |
| Escape / right click | Back; Files right click opens item Actions |
| Tab | Task Switcher |
| F / Ctrl | FIRE / app action |
| Left click | Open a menu row or visible control |
| Note | Type US ASCII; Shift, Backspace and Delete work |
| Paint | Hold left mouse button to draw |

## Files and Disks

Open Apps > Files > Disks. C: is the existing 320 KiB internal LittleFS volume;
D: appears when a USB mass-storage drive is detected. Connect the drive to
native USB/OTG; keep UART/programming connected for power. A powered single
USB hub can share the native host port with supported keyboard/mouse devices.
Only one USB storage volume is exposed as D: at a time.

Select C: or D: with UP/DOWN and OK, or click it. D: mounts FAT16/FAT32 using
the same driver as Freslest Update, with formatting explicitly disabled. A
mount error remains visible on the disk page; OK retries. ExFAT/NTFS are not
supported. A disconnected drive disappears; if a D: folder/text/action was
open, Files returns to Disks and discards old entries. Reconnecting starts at
the new drive root. An absent drive is never replaced with C: silently.

FIRE / F on Disks safely unmounts D: and shows Safe to remove. OK can remount
it while still plugged in. If native USB is in PC-transfer device mode,
Disks shows FIRE: Switch to USB host; that action restarts the board. Physical
buttons remain usable without keyboard/mouse. The updater and PC-transfer
screens still use their existing separate boot modes.

Within a disk, folders appear first. Left click / OK opens folders or text;
binary files show Details. FIRE / F / Actions and right click on a row open
Open, Details, Delete. Deleting asks again with Cancel selected by default;
only empty folders can be removed. BACK goes to a parent; at a volume root it
returns to Disks, and BACK on Disks returns to Apps. LEFT/RIGHT pages lists/text.
Lists are bounded to 128 entries and names to 64 bytes, with a limited-list
notice. Text preview uses 768-byte pages and safe display characters.

C:/games/nes contains installed ROMs and C:/note.txt contains Note. D: browsing
is separate; it does not copy/import a ROM automatically or play USB MP3s.
DFPlayer microSD, WAD, saves and raw OTA partitions are not writable files here.
Entering Files is nondestructive; no existing disk is automatically formatted.
Filesystem access holds a mutex; USB disconnect callbacks only publish removal
and queue cleanup, so a live file read is not freed from another task.

## NES

| Keyboard | NES |
|---|---|
| Arrows / WASD | D-pad |
| X / Z | A / B |
| Enter / Right Shift | START / SELECT |
| Escape | Return to Library |

Fullscreen is the LAST row INSIDE NES Library, below USB Transfer to PC. It is
saved in settings and works with mouse or OK. Game ends with NES Library.

To import: NES Library > USB Transfer to PC (or physical FIRE), wait for the
restart, connect the native port to the PC, copy .nes to the FRESLEST drive root,
EJECT in Windows and wait for Imported/Unchanged/Rejected. Physical OK returns
to NES, restarting into host mode; reconnect your input hub. Physical BACK
cancels transfer. UART supplies power throughout. The UI retains import errors
and supports FIRE retry. Interrupted copying does not replace existing ROMs.

## Doom

| Input | Action |
|---|---|
| W / S | Forward / back |
| A / D | Strafe left / right |
| Arrows | Movement / turn; menu movement |
| Mouse horizontal movement | Turn |
| Left mouse / Ctrl | Fire |
| E / Space / right mouse | Use |
| Shift | Run |
| 1-7 | Select weapon |
| Tab | Automap |
| Enter / Escape | Confirm / menu |

Doom has no vertical free-look. Keyboard and mouse work together through the
hub. Mouse left click confirms Doom menu choices; use arrows for selection.
Doom > Quit Game returns to Freslest. Physical controls are preserved.

## Install/update

Use the complete ZIP and flash_all.bat COM5 (replace the port) for OS + Doom.
Do not use --with-files for an existing installation. ROMs, Note, settings and
Doom saves are retained. The established small update ZIP replaces OS only;
6.10 OS alone cannot add USB code to an older Doom partition. USB updating
and PC transfer use physical buttons; desktop HID is not active in those modes.

Firmware builds/software tests do not constitute physical compatibility or FPS
measurements. Check System Info device counts and your hub's VBUS supply first
if a keyboard/mouse does not appear.

## Display and startup correction in 6.10

The DAC bit order is GPIO8/13/14/2/42/40/39/48 (D0..D7). OS cursor
composition before DMA remains. Composite now uses continuous DMA and a bounded
first-frame startup wait. Settings > Display > TV alignment adds saved image
size and position, shared with Doom. Reduce Size to make space before shifting.
Doom gameplay, mouse sensitivity, buttons and menu behavior are preserved;
its first-frame startup check is corrected. Install the complete 6.10 PC
package for the Doom fix; the small USB update ZIP contains OS only.
See COMPOSITE_DISPLAY.md.

## BLE keyboards and mice in 6.10

See BLUETOOTH_INPUT.md. BLE inputs share the OS/NES input state with USB inputs;
a disconnect releases only that peripheral's keys/buttons. System Info counts
include both transports. Doom keeps USB/physical input in this release.
Composite UI is 480x360 scaled uniformly to 320x240, with matching pointer bounds.
