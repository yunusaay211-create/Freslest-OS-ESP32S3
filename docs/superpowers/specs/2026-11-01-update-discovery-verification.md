# Display-preserving update delivery verification

Firmware source: `3e69c901cf7b2dad71007c2384a554194379b971`.
The [OS-only build](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/41324556767)
passed in ESP-IDF5.4.4. Doom and Windows build jobs were skipped. Production
changes are confined to `common/usb_update`; apps, video/audio implementations,
GPIOs, partition map and menu dimensions remain unchanged.

| OS image | Bytes | Free in existing partition |
| --- | ---: | ---: |
| ILI9488 | 1,762,048 | 7,424 |
| ILI9341 | 1,762,624 | 6,848 |
| VGA preset | 1,762,880 | 6,592 |

Host and CI checks cover both old/current C readers for each controller,
explicit target selection and changed metadata, 138 output/board/display
cases, both OS slots with 199 Doom cutpoints each, 205 existing OS cutpoints,
36 IDF rollback cuts, 14 profile package tests and eight installer tests.
Real package checks cover all four controller/output combinations and both
OS slots. The delivered set lists exactly one matching OS and correct Doom.

The initial CI's LCD game artifacts were older than the current installer.
An exact game-reuse regression failed on that collection. Delivery was then
assembled from the current installer's three original Doom images, with new
OS images from the successful build. The regression passed and actual old/new
reader, output matrix and native writer checks were repeated on this exact
three-file set. The build tool now produces OS images only; release assembly
requires the current installer rather than an older game artifact.

The new full installer changes exactly seven archive entries: three OS images,
three checksum manifests and VERSION_REPORT. All 1,233 other entries, including
every Doom image, Windows runtime/PYZ and 112 SD sounds, retain packed bytes and
order. All profile/audio hashes and 1,229 Windows extraction paths pass.

- `Freslest-Update-1.1-OS-Doom.zip`: 3,701,774 bytes;
  SHA256 `f221ec88cdfbc0edd3a9120d6ae9ec52d48833de5421369cc3a08a7ae4c51b6e`.
- `Freslest-Installer-1.1-OS-Doom-Update.exe`: 103,981,528 bytes;
  SHA256 `8d82b60e6f12bae038d6bdf5af5d45ac47523ef2502b713cfba3e30211ece9f9`.

The repacked Windows executable was not launched on Windows during this task;
its executable runtime is unchanged. No ESP32-S3 hardware was attached for
this verification. LCD/VGA operation on the user's device remains a physical
test distinct from these automated checks.
