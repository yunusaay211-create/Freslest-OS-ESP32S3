# Freslest USB Update Formats 2 and 3

## Current display-preserving update collection

Extract the OUTER `Freslest-Update-1.1-OS-Doom.zip`. Copy its `UPD88.ZIP`,
`UPD41.ZIP` and `DOOMVGA.ZIP` files UNEXTRACTED to the USB drive root.
Older OS versions list only the OS package matching their LCD controller.
Apply that OS update, reboot and allow at least ten seconds for startup
confirmation. The new updater lists OS and Doom as separate choices.

`UPD88.ZIP` carries the ILI9488 OS and preserved LCD Doom, while `UPD41.ZIP`
carries the corresponding ILI9341 images. Both bootstrap OS images have LCD
defaults and retain the saved `fl_video/output_v2` setting, so a previously
saved VGA output remains VGA. `DOOMVGA.ZIP` contains only the existing verified
native RGB332 VGA Doom. Fresh VGA installation uses a separate VGA-preset OS
image in the full installer.

Optional manifest `display` is `preserve` for OS, `lcd` or `vga` for Doom.
Explicit target inspection never falls back to another component. Runtime
inspection filters Doom by the current initialized display output. OS and LCD
Doom always require the same LCD board identity. An explicitly tagged VGA Doom
for the approved ILI9341 VGA build can also run on an ILI9488 OS when the actual
current output is VGA; it does not change the OS's LCD controller or settings.
Unknown boards, malformed/duplicate display tags and wrong output are rejected.
Packages without `display` keep the preceding board-strict legacy behavior.

The full installer replaces only the three OS images and their metadata.
Its Doom binaries, Windows runtime, audio files and display configuration
sources are retained. Source-only updater changes require regenerating the
linked OS images; no Doom or Windows GUI firmware build is performed.

The following sections document the unchanged integrity and write rules.

Identity: format freslest-usb-update, format_version 2, updater_min 2,
board esp32s3-n16r8-ili9488-buttons, layout freslest-s3-ab-v2.
Version matches the ESP descriptor. Image: firmware/freslest_os_s3.bin,
maximum 0x1b0000 bytes, whole-file SHA256, project freslest_os_s3, chip ID 9,
16 MB flash, valid ESP segment checksum and appended image digest.

One manifest is accepted at ZIP root or under the complete project's root.
No archive is extracted to disk and no package field supplies a flash address.
ZIP local/central metadata, bounds, JSON keys, size, CRC and SHA are checked.
Stored and DEFLATE entries are supported; ZIP64, split/encrypted archives,
unsafe paths and duplicate relevant entries are rejected.

OS A/B are OTA_0/OTA_1, Doom is OTA_2. The updater explicitly selects the
non-running OS slot. It stages/validates before any erase, commits WRITING,
uses IDF OTA begin/write/end, hashes read-back, commits TRIAL and selects boot.
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE is enabled in both builds. IDF changes
NEW to PENDING_VERIFY and an unconfirmed reboot to ABORTED; the old VALID OS
record remains available. Confirmation requires >=10 seconds and >=60 healthy
UI frames; a 45-second startup deadline rejects a stalled first boot.

Power loss during writing keeps the old OS. Loss/failure during the unconfirmed
first boot selects the previous valid OS at the next power-on. Accepted images
persist across later resets. FIRE offers a confirmed restore of the last
validated OS; only one previous version is kept, until the next update starts.
These checks do not detect every bug after startup confirmation.

The first transition from layout v1/4.x requires the complete PC installer.
Format 2 updates OS and its updater, retaining Doom/WAD/user data.
SHA256 verifies integrity, not publisher identity. See README.md for OS steps.

## Native Doom extension (format 3)

The updated OS also recognizes `freslest-doom-update.json`. It requires
`format_version: 3`, `updater_min: 3`, and `target: "doom"`. The existing
format, board, layout, version and image-object fields retain their meanings.
The only image path is `firmware/freslest_doom_s3.bin`; its project must be
`freslest_doom_s3` and its maximum size is `0xe0000` bytes. No manifest selects
a flash address. The confirmed running OS validates the fixed `doom` OTA_2
partition at `0x20000`, stages and checks the whole image, writes with IDF OTA,
then checks a SHA256 readback. It restarts the same OS without changing the OS
update journal, previous-version pointer or boot selection.

Doom has a single slot. A failed or interrupted Doom write can leave Doom
unavailable until the ZIP is applied again; the confirmed OS remains available
to retry. WAD, filesystem and partition changes still require the PC installer.

## One ZIP for installed older OS versions

`python tools/make_doom_update_zip.py --os OS.bin --doom Doom.bin --output Update.zip`
creates a VGA package containing both manifests and both application images.
The OS image must carry `FRESLEST_UPDATE:OS_AND_DOOM_V3`. The old updater reads
only `freslest-update.json` and installs that OS using its existing A/B rules.
After the new OS has opened normally and confirmed startup health, selecting
the same ZIP again installs Doom. The new updater prefers the distinct Doom
manifest whenever present; a malformed or duplicate Doom manifest fails the
package rather than falling back to an OS write. Standalone legacy format-2 OS
ZIPs remain supported.

Copy the ZIP, unopened, to the USB drive root. Select it in **Freslest Update**.
For an older OS, complete the first OS update and healthy restart, then open
**Freslest Update** and select the same ZIP a second time for Doom. The full
Windows installer with the VGA profile installs both images in one operation.
