# VGA timing rollback

User requested the old resolution and clock after testing VGA252.
Historical source 4f7b3436c1e2ba4754a25ae739dffe864f0f6a65 sets PCLK to 25,175,000 Hz and uses 640×480 active pixels. Restore those values for all new OS/Doom VGA outputs. Keep the 96/48/16 horizontal and 2/33/10 vertical sync/porches, RGB332 pin mapping, fit preference, buffers, C4 test and C5 Doom fixes.

The installer retains VGA as a separate profile, identified by VGA25175_25175000. Its new NVS preset key activates VGA once on migration; subsequent output selections and BACK recovery remain respected. Installer labels show 640×480 / 59.94 Hz and 25.175 MHz. No 16 MHz mode is added.

Regression: actual backend test failed against the old target clock before implementation. Existing preset tests cover startup, user choice, BACK and NVS errors. Run all six source firmware builds and native Windows tests before delivery. The supplied C4 photograph shows the raw test, with glare obscuring part of it; this timing rollback does not establish that the physical color issue is fixed.

## Verification

- Source commit: 9dc10a95d30cc5f945f5711044a236f91cfc2bbb.
- Successful firmware + Windows workflow: 40691429211.
- Firmware artifact 12818285683, SHA256 27de5a5dc1156ffcdf2690342d04cc41e289a7fa633e9613323061d4356d0ae0. All 21 segment hashes and six ESP32 image checksums/digests verified; every image fits its slot.
- Windows artifact 12816649540, SHA256 ae0f9c8d921901add35e9fc64d14d18d83c01548015e19d4e18755cce3a65ebb.
- Installer: Freslest-Installer-1.1-VGA25175.exe, 35,245,092 bytes, SHA256 a3de66b36bff622bb3a804d2d6ee9889fcb01e0efcb42d56c406eb0e43efe77a.
- Local suite: 56 cases, 48 passed, 8 desktop skips. Native Windows: 56 cases, 45 passed, 11 Linux sanitizer skips; 9 Viewer cases passed. TR/EN installer and Viewer native EXE launches passed.
- Physical target-clock measurement and final TV color correction remain unverified. No main merge or public release.
