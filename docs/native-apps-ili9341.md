# ILI9341 native app rendering — 2026-10-27

User confirmed bad app text/visible lines, while Doom orientation now works. First identified Calculator/Note/Clock, then requested the same correction for every app and a rebuilt installer.

All Apps-menu entries now select the native320x240 framebuffer: Calculator, Note, Paint, Clock, Camera, Files, Browser, and already-native Settings. Calculator/Note/Clock/Paint use explicit compact layouts; Files/Browser/keyboard map legacy geometry to native coordinates while drawing whole font glyphs. Text that exceeds a label has an explicit ellipsis. Browser HTML wraps at48 columns and Files text pages use12x46 bytes so no preview bytes disappear between pages. Pointer targets share the transformed geometry. Wi-Fi/browser text entry no longer creates a480x320 intermediate framebuffer.

Camera JPEG placement now uses full JPEG dimensions, fits the viewport with the decoder's supported scale, and uses the callback's already-offset coordinates once. Previously the first MCU tile was used to choose a center and the origin was added twice. Camera text/footer use actual dimensions.

Doom's raster/orientation code is unchanged by this app correction. The preceding VGA starvation/recovery candidate remains included. Other game canvases retain their compatibility path; this correction concerns Apps-menu entries, their settings and text entry.

Actual source tests: tests/run_native_app_tests.py covers native/legacy Calculator, Note, Clock, Paint, Camera coordinates/decoder sizing and keyboard submit/cancel. tests/run_native_views_tests.py runs the actual Files/USB and standalone Browser flows at both sizes and verifies native file paging reconstructs every byte. Hooked into the Linux LCD regression. Hardware verification remains pending.

Adapter review found a color-type promotion bug: passing theme uint16_t colors through uint32_t/unsigned parameters makes LovyanGFX treat RGB565 as RGB888. A regression using the actual bundled color_conv_t reproduced white0xffff becoming0x07ff. App/settings geometry adapters now preserve the caller color type through templates; RGB565 and explicitly typed RGB888 both compare equal to direct conversion.

Cross-build also exposed LGFX width() returning int32_t/long on the ESP target versus int in the original host double. Text capacity now selects std::max<int> explicitly; the real-converter regression uses a long width return and reproduced then passed this compile boundary.
