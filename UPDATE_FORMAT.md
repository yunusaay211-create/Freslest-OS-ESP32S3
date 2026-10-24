# Freslest USB Update Format 2

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
This USB format updates OS and its updater, retaining Doom/WAD/user data.
Doom, WAD or partition changes require the PC installer. SHA256 verifies
integrity, not publisher identity. See README.md for exact install steps.
