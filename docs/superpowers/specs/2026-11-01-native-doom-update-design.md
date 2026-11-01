# Native Doom audio update

The user reports that correcting the VGA wiring restored Doom colors. Keep the
verified RGB332 backend, pin mapping, 640x480 VGA timing at 25.175 MHz (800x525
totals) and 480x320 OS canvas unchanged. Deliver the existing tested music
restart fix through an on-device USB Update ZIP and a full Windows installer.
The user selected the on-device ZIP route and authorized updating the existing
source branch `codex/audio-sd-recovery`.

## Package compatibility

Existing format-2 updaters only install OS A/B. One ZIP contains the legacy OS
manifest/image and a distinct format-3 Doom manifest/image. An old updater
installs the new OS, which must advertise the native Doom update capability.
After the OS boots and confirms health, the user selects the same ZIP again.
The new parser prefers the Doom manifest and installs Doom. There is no saved
automatic-continuation state. Legacy standalone OS packages continue to work.

## Write boundaries

Only a confirmed running OS with the complete expected layout may install.
Format 3 targets the fixed `doom` OTA_2 partition at 0x20000, size0xe0000.
Manifest fields never specify an address. Reinspection compares target as
well as version, size and SHA. Validate the whole staged image before erase,
write with IDF OTA, then verify a whole-file SHA readback.

Doom installation, cancellation and errors do not touch the OS update journal,
previous-OS pointer or boot selection. This includes the UI's return-to-OS
path. Existing updater entry REQUEST-to-IDLE bookkeeping can still run before
the user selects an image. Restart returns to the same confirmed OS. Doom has
one slot: interruption can require applying the ZIP again, with OS available
to perform that retry. This is not an A/B rollback promise for Doom.

## Installer and verification

Build both VGA applications with ESP-IDF5.4.4 and enforce their actual partition
limits. OS has little free space; do not alter the partition map to make it
fit. Repack the verified full Windows installer by changing only its VGA OS,
VGA Doom and two profile metadata entries. Preserve the runtime, other display
profiles and all112 SD sounds byte-for-byte. Verify extraction paths and all
profile/audio checksums after repacking.

Exercise both old and current real C parsers on the actual built ZIP. Test the
native writer from each OS slot, including every injected write interruption,
bad images, wrong targets/layouts, pending OS, failures and cancellation.
Distinguish automated host/build evidence from physical device verification.
