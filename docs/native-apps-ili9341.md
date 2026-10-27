# ILI9341 native app rendering — 2026-10-27

User confirmed bad app text/visible lines, while Doom orientation now works. First identified Calculator/Note/Clock, then requested the same correction for every app and a rebuilt installer.

All Apps-menu entries now select the native320x240 framebuffer: Calculator, Note, Paint, Clock, Camera, Files, Browser, and already-native Settings. Calculator/Note/Clock/Paint use explicit compact layouts; Files/Browser/keyboard map legacy geometry to native coordinates while drawing whole font glyphs. Text that exceeds a label has an explicit ellipsis. Browser HTML wraps at48 columns and Files text pages use12x46 bytes so no preview bytes disappear between pages. Pointer targets share the transformed geometry. Wi-Fi/browser text entry no longer creates a480x320 intermediate framebuffer.

Camera JPEG placement now uses full JPEG dimensions, fits the viewport with the decoder's supported scale, and uses the callback's already-offset coordinates once. Previously the first MCU tile was used to choose a center and the origin was added twice. Camera text/footer use actual dimensions.

Doom's raster/orientation code is unchanged by this app correction. The preceding VGA starvation/recovery candidate remains included. Other game canvases retain their compatibility path; this correction concerns Apps-menu entries, their settings and text entry.

Actual source tests: tests/run_native_app_tests.py covers native/legacy Calculator, Note, Clock, Paint, Camera coordinates/decoder sizing and keyboard submit/cancel. tests/run_native_views_tests.py runs the actual Files/USB and standalone Browser flows at both sizes and verifies native file paging reconstructs every byte. Hooked into the Linux LCD regression. Hardware verification remains pending.
