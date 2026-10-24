# Freslest OS 6.13: Recovery and Deep Sleep

## If the OS will not open after Wi-Fi was saved

Unzip the complete package. In an ESP-IDF/Python command prompt, enter its
`Freslest-OS-Doom-S3` directory and run:

```bat
repair_os.bat COM5
```

Use the programming/UART port, and replace COM5 if Windows assigned another
port. Close Serial Monitor first. This repair is for an existing Freslest
5.x/6.x A/B installation. It installs the new OS in slot A and resets OTA boot
selection. It retains Doom, WAD, LittleFS/NES ROMs, NVS settings and Doom saves.
It resets the old OTA selection/history; a previous version becomes available
again after a successful USB update. Do not use `--with-files` to fix startup.
For a first installation use the complete install instructions in README.md.

Normal startup shows the menu before starting saved Wi-Fi (2.5 seconds later)
and Bluetooth (6.5 seconds later). The two stacks no longer initialize at once.
Wi-Fi/LwIP and Bluedroid prefer PSRAM for eligible allocations, preserving
internal RAM needed by tasks and DMA. Wi-Fi retries have one owner, with a
20-second interval; invalid credentials and returned initialization failures
are recorded without deliberately resetting the OS. This addresses startup
ordering, memory pressure and reconnect races. A board-specific panic or
power fault still needs the actual UART log for diagnosis.

## Enter Recovery

Hold physical LEFT + RIGHT (GPIO6 + GPIO7), press RESET, then release the
buttons. Recovery keeps the selected LCD/AV output; BACK + RESET still forces
LCD. UP + RESET still opens AV Lab. Those existing display shortcuts are
unchanged, and AV pins/timing are unchanged.

Three interrupted startups enter Recovery automatically on the next startup:
boot 1, reset 1, reset 2, reset 3 -> Recovery. An interrupted startup means a
reset before the OS has remained healthy for 20 continuous seconds, following
its normal OTA startup check and scheduled radio startup. The count is stored
in NVS and survives power removal. A healthy boot clears it. Intentional
software restarts for USB-role changes, Doom, or updater handoffs, and
successful deep-sleep wakes do not count. A panic/watchdog reset counts even
if the preceding startup had already completed. Repeated failures before the
new OS reaches its setup code cannot be handled by this application recovery;
the ESP-IDF bootloader rollback or serial repair is then required.

Recovery initializes only display and onboard buttons. It does not initialize
Wi-Fi, Bluetooth, their setup portal, NTP, USB HID/MSC or the normal app loop.
Use physical UP/DOWN and OK; LEFT/RIGHT can change the radio setting rows.

1. **Continue Freslest** resumes normal startup using the saved settings.
2. **Freslest Downdate** restores the other complete, valid Freslest OS image,
   after confirmation. Wrong project, invalid/aborted image, bad image digest,
   blank slot and incorrect partition size are rejected. It never selects Doom.
   If no eligible image exists the row is grey; selecting it does nothing.
3. **See Log** shows the reset cause, previous boot stage, interrupted-boot
   count, free/largest internal RAM, last saved application error and recent
   warning/error lines. UP/DOWN scrolls; BACK returns.
4. **Wi-Fi after exit: ON/OFF** saves only the setting.
5. **Bluetooth after exit: ON/OFF** saves only the setting.

**Turning a radio ON in Recovery never starts its hardware there.** Turning it
OFF persists that choice. After Continue, normal Freslest applies the selected
settings. Network passwords and Bluetooth bonds are retained. Recovery errors
have Retry, Recovery and **See Log as the third action**. Failed settings writes
do not change the displayed setting. Recovery remains open until you choose
Continue or a confirmed Downdate; BACK does not accidentally resume a boot loop.

Recent warning/error lines use RTC memory and normally survive software and
watchdog resets. Full power loss may clear them; the boot stage and last
application error remain in NVS. This viewer is not a full exception/core dump.
For the complete panic traceback, record the programming/UART serial output
at 115200 baud. No password, key input or HTTP page content is added to this
viewer by the diagnostic capture.

## OTA safety

The original A/B layout, ZIP v2 format, SHA/image checks, 45-second trial
watchdog, 10-second/60-frame health confirmation, and bootloader rollback
remain intact. A failed unconfirmed OTA trial rolls back immediately on the
next reset; it does not wait for three failures. Recovery does not confirm a
trial image merely by displaying its menu, and starts no trial watchdog while
the user is inspecting offline Recovery. Choosing Continue starts the normal
trial check. A reset while inspecting an unconfirmed trial lets the bootloader
restore the previous valid OS.

An OS-only update cannot add Recovery code to an older inactive OS. If a new
trial fails back to 6.12 or earlier, that older version retains its original
behavior. Install 6.13 successfully before relying on its recovery features.
Doom remains the original 6.10 source/binary and is never updated by this ZIP.

## Five-minute inactivity sleep

After startup is healthy, five minutes (300000 ms) without button, keyboard or
mouse activity enters actual ESP32-S3 deep sleep. A stationary connected mouse
does not reset this timer; key/button holds, taps, typing and pointer motion do.
Wi-Fi and Bluetooth stop, USB/AV clocks stop with the chip, and the LCD backlight
is turned off. Saved network settings and the RTC clock are retained. A USB-
powered board/display/DFPlayer can still draw current; this is CPU deep sleep,
not a physical disconnection of every peripheral's power supply.

Press **physical OK (GPIO15) or BACK (GPIO16)** to wake. Either RTC pin going LOW
wakes the ESP32-S3. Release the button; Freslest restarts normally and restores
the selected display. In particular, BACK used to wake from sleep does not force
LCD mode. USB/Bluetooth keyboards and mice cannot wake this deep-sleep mode.

The timer pauses during Recovery, OTA/PC USB transfer, a mounted/present USB
drive, phone setup portal, active Browser download, Camera, and the Note/Paint
editors (which may contain unsaved RAM content). Busy USB cleanup/mounting also
blocks sleep; a lock-protected sleep gate prevents a late USB connect from
mounting after the sleep decision. Five fresh idle minutes are required once a
blocking operation ends. Leave an editor or stop the setup portal to allow sleep.
An unsuccessful wake-pin setup leaves the device awake. Radio shutdown failure
restarts into offline Recovery rather than resuming with a partly disabled stack.

This applies to Freslest OS/NES. The separate Doom firmware is unchanged and
does not gain a new automatic-sleep policy. Deep sleep is a reboot, so running
app/game RAM state is not resumed after wake.

## Validation limits

The actual recovery/sleep code, Wi-Fi lifecycle, USB sleep interlock, keyboard/
mouse activity path, existing app/NES behavior and OTA power-loss/rollback paths
have host tests. The OS is cross-compiled for ESP-IDF v5.4.4 / ESP32-S3 N16R8.
Physical cold boot, RF coexistence, wake electrical behavior, current draw and
TV/LCD output have not been tested on your board here. Use See Log/UART if the
startup problem persists; a software test cannot establish its physical cause.

Official API references used for this implementation:
- https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32s3/api-reference/system/sleep_modes.html
- https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32s3/api-reference/system/ota.html
