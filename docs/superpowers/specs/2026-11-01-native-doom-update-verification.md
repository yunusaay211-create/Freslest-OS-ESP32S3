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

## Real firmware and delivered packages

The [native update workflow](https://github.com/yunusaay211-create/Freslest-OS-ESP32S3/actions/runs/41300958453)
passed with source commit `b3dc1327de102ad39991fec47bb7c2bb11fdaa32`.
The actual built ZIP passed 77 native/legacy C reader cases. Both applications
fit the existing partition map without changing it:

| Application | Bytes | Partition bytes | Free bytes |
| --- | ---: | ---: | ---: |
| VGA OS | 1,762,288 | 1,769,472 | 7,184 |
| VGA Doom | 890,832 | 917,504 | 26,672 |

- OS SHA256: `fb7925320b28869ffad4fdffd312c10c0eb03aea08c51d992e8014aee21f8c23`
- Doom SHA256: `849ba46c77ce38cc8815d5d3cad24f044a9bdad147e27c5870dd4c1006bc255e`
- `Freslest-Update-1.1-Doom-Audio.zip`: 1,609,774 bytes;
  SHA256 `7ecd50042014e2d159dfbc619c061f48f757054d46fcde317f114107e4ef4dde`.
- `Freslest-Installer-1.1-Native-Doom-Update.exe`: 103,977,970 bytes;
  SHA256 `ba41887130104483a43bb05692534d0988ca8b3efe03612919813f1170d9b3fc`.

Installer verification replaced exactly four entries: VGA OS, VGA Doom, the
VGA checksum manifest and the version report. The other 1,236 compressed
entries, their order and the Windows executable prefix are unchanged. All
three profile manifests, 112 audio hashes and 1,229 Windows extraction paths
passed. The existing Windows runtime is preserved; this repacked EXE was not
launched on Windows during this request. Real Tk desktop GUI tests ran under
Xvfb, as recorded above. No ESP32-S3 hardware was attached for audio testing.
