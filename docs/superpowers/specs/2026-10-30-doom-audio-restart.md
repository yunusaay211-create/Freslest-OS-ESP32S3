# Doom audio restarts and unresolved VGA color report

User reports orange Doom colors and music restarting three times after the native RGB332 installer.

Two replay defects were reproduced in the actual C UART state machine before fixing them:

- Status-zero responses caused three PLAY commands for one requested song. The start-status query is now diagnostic only; a status response has no song/request identifier and cannot justify interrupting playback.
- First positive SD confirmation after an already-sent PLAY caused a second PLAY. Unknown-to-online confirmation now preserves playback. Actual removal and reinsertion still resume the current requested track once.

The recovery test also covers booting without SD: a complete `0x3F` source mask without the SD bit records the card as absent, then insertion starts the current request exactly once. USB-only insert events and a zero file count do not remove an active SD.

The eight `doom_updater` sanitizer tests and full Doom host suite passed. The latter uses the packaged9,717,304-byte IWAD temporarily; no WAD is added to source. Test wiring now includes the private Doom presentation header. Review found the no-card-startup regression during implementation; its failing regression and minimal source-mask correction are included.

No production VGA, palette, OS or shared runtime files changed. VGA remains640×480 at target25,175,000Hz with800×525total timing; OS canvas480×320 and GPIOs are preserved. Build only Doom VGA and replace only that payload plus its checksums/report in the existing complete installer; retain all112SD sounds.

The orange color issue remains unresolved. Native palette/gamma tests and RGB332 word tests pass, but they do not establish physical pin levels or analog colors. The next required observation is the current firmware's raw C4 pattern: select Doom, press OK, then immediately hold RIGHT(GPIO7) during Doom startup. It appears for10seconds; BACK exits early. This bypasses Doom palettes and distinguishes a palette/game-image issue from the subsequent transport/output path. Serial route diagnostics are already printed after all Doom peripherals initialize.

Do not claim physical color or music behavior confirmed without a device run. Manufacturer protocol reference: https://github.com/DFRobot/DFRobotDFPlayerMini/blob/master/DFRobotDFPlayerMini.cpp

## Built deliverable

ESP-IDF5.4.4 build and all eight native CI tests passed: https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/41093620784
Firmware source:`ef8d944ab3665ae3b096fc6f5f536346436403e1`;890,832bytes;SHA256:`a4fe45b9d01da490c133df9813e47f76b0c5b51b21ccd7b76be53ebf6000de52`.
Complete installer:`Freslest-Installer-1.1-Doom-Audio-Fix.exe`;103,976,878bytes;SHA256:`52c0cbd6a5fbfaa07631eff56fe65a90bfe01ba370171e4bca0cf9af67afa073`.
Compared with the preceding RGB332 installer, only VGA Doom, its checksum manifest and release report changed. All1,237other compressed entries, runtime/PYZ and bootloader prefix are identical. All1,229Windows extraction paths,112audio hashes and all firmware-profile hashes passed validation.
