# VGA timing rollback

User requested the old resolution and clock after testing VGA252.
Historical source 4f7b3436c1e2ba4754a25ae739dffe864f0f6a65 sets PCLK to 25,175,000 Hz and uses 640×480 active pixels. Restore those values for all new OS/Doom VGA outputs. Keep the 96/48/16 horizontal and 2/33/10 vertical sync/porches, RGB332 pin mapping, fit preference, buffers, C4 test and C5 Doom fixes.

The installer retains VGA as a separate profile, identified by VGA25175_25175000. Its new NVS preset key activates VGA once on migration; subsequent output selections and BACK recovery remain respected. Installer labels show 640×480 / 59.94 Hz and 25.175 MHz. No 16 MHz mode is added.

Regression: actual backend test failed against the old target clock before implementation. Existing preset tests cover startup, user choice, BACK and NVS errors. Run all six source firmware builds and native Windows tests before delivery. The supplied C4 photograph shows the raw test, with glare obscuring part of it; this timing rollback does not establish that the physical color issue is fixed.
