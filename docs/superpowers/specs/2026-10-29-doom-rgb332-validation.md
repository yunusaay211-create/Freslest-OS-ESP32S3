# Doom RGB332 validation, 2026-10-29

Firmware source: `d5f2f6223382e0a66cf2e19bd4a391252e7ac904`.
Successful ESP-IDF5.4.4 ESP32-S3 build: https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/40935673522

- Seven Doom updater/runtime/native RGB332 tests passed locally and in the firmware job, including ASAN/UBSAN, all256 colors, five fit values, bounds/cursor/palette checks, dropped-frame retry, unchanged OS NVS, and1,000 overdue demo frames.
- Actual Doom gamma0..4/RGB888/LCD endian tests and neutral-menu/effect-palette tests passed.
- Firmware job verified OS/shared production sources against baseline `7746435276511e3b6929072ccc8e080f922a523a` with no changes.
- Firmware:890,592bytes, partition917,504bytes; SHA256 `efd3ce674fa61e22404da4cf951a16aa98a7097aeca72db9dffde4668045a810`.
- esptool4.11.0 independently reports a valid image checksum and validation hash for all six ESP32-S3 segments.
- Required native RGB332,25.175MHz and reset-reason markers are present. Timing remains640×480,800×525total; GPIO order and OS480×320canvas are preserved.

Complete installer: `Freslest-Installer-1.1-Doom-RGB332.exe`,103,976,604bytes.
SHA256: `377a340f6ee33d5c0c841c2074f276d20cfed9319e0dc8fdb965f1dc8f2d39c3`.

Base: existing Audio-SD-Fix installer version2, SHA256 `ddb1d47dbecc105fabc8fc4a4fbf934bdae9f0f98ba7d86215f216f1eb9cb1f3`.
The archive reader/writer reconstructed this real1,240-entry Windows executable byte-for-byte before replacing its payload.
Only VGA Doom, its profile SHA256.json and VERSION_REPORT.json changed. All1,237other compressed entries, Windows prefix/runtime/PYZ, OS images, LCD Doom images and SD assets remain byte-for-byte identical.
All1,229 Windows extraction paths, all112 MP3 checksums and all three firmware-profile checksum manifests passed validation.

Choose the VGA RGB332 profile in the complete installer to install the new Doom image. Its SD sound installation step remains the existing verified installer code.

No ESP32-S3/VGA hardware is attached here. The new transport is built and tested; the photographed color issue and the device's actual reset cause are not claimed resolved. The overdue-demo scheduling defect was reproduced and corrected in software. Boot logs now distinguish task watchdog, interrupt watchdog, panic and brownout, and normal Doom initialization prints the actual VGA GPIO routes.
