# Freslest 6.10: AV controls

The current output driver is Freslest AV Engine 0.1.1. See AV_ENGINE.md for the
complete signal/transport design and the static board test. Earlier 6.06-6.08
filter-only experiments did not establish a physical fix for the supplied TV.

Settings > Display > AV colors retains Hue, Color, Detail and Reset rows.
UP/DOWN chooses a row, LEFT/RIGHT changes it, OK saves and BACK cancels.
FIRE previews Hue 0 / Color 100 / Detail Clean. Mouse controls also work.
The labelled RGB bars and neutral grey ramp remain.

Sharp now uses the new engine's horizontal interpolation. Clean adds a
four-sample unmodulated-phase carrier trap. These are not the old 6.08 filters.
Color 0 removes active chroma and colour burst for a true monochrome test.
The old NVS color_v1 fallback and current color_v2 settings are retained.

For an isolated physical test, hold UP while pressing RESET. The initial AV Lab
scene is monochrome and stationary, before storage, USB/BLE, sound and game
services start. Use UP/DOWN to compare grid / bars / text / white. BACK returns
after button release. Lab mode does not confirm a pending OTA trial.

Only the full PC installer updates the engine in both OS and Doom. The small
v2 USB ZIP replaces OS only. 6.10 uses the requested VIDEO OUT order: 48,39,40,42,2,14,13,8.
The waveform and colour-control algorithms retain the 6.09 design.
No TV picture-quality or FPS claim is made from the software tests.
