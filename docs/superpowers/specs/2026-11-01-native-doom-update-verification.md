# Native Doom update verification

Host verification, 2026-11-01:

- 75 real C native/legacy package cases under ASAN/UBSAN.
- 57 dual-bundle builder cases, including checksums, capabilities, profile
  markers and application partition limits.
- Native runtime tests from both OS slots: 199 interruption points each,
  cancellation/unplug, corrupt/replaced packages, target/layout/startup
  rejection, allocation/write/read errors and target-visible UI.
- Existing OS parser/runtime: 36 fixtures and 205 interruption points.
- Unmodified ESP-IDF5.4.4 selector/bootloader functions: 36 interruption points.
- Four installer CArchive preservation/rejection tests.
- Eight Doom audio/RGB332/runtime tests, including one PLAY command after
  ambiguous status and late first SD confirmation.
- Desktop suite with restored verified release fixtures and real Tk under
  Xvfb: 79 top-level tests (one Windows-specific volume check skipped) and
  nine Viewer tests passed. SD wizard/copy/cancel, language and display
  selection tests are included.
- Independent source review: no Critical, Important or Minor findings.

The real cross-build and actual-ZIP checks run in the dedicated
`Native USB Doom update` workflow. Its `release.json` records the source
commit, image sizes, free partition bytes and SHA256 values. Firmware build
evidence does not establish the music result on a physical device. The user
reported that correcting a physical connection restored Doom colors; no
video, audio, GPIO, menu or timing source was changed for the delivery route.
