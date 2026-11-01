# Freslest USB Update Formats 2 and 3

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
