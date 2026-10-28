# C5 Doom menu palette and gamma fixes

The user requested implementation of the two confirmed Doom review findings, a GitHub source update, and a freshly built Windows installer.

## Menu colors

Previously ST_doPaletteStuff only halved the damage palette index when the menu was open. That still selected red PLAYPAL entries 1 through 4. In a paused single-player game, damage counters stop decreasing, so the red menu could persist.

The menu now selects normal PLAYPAL 0 after computing gameplay effects. Closing it recomputes the damage, pickup, radiation or berserk effect and restores the appropriate palette. It does not clear player counters or disable gameplay feedback. The palette cache is updated normally, so a static menu does not repeatedly upload its palette.

## Gamma

The ESP32 I_SetPalette previously ignored usegamma for both outputs. It now applies the engine's existing five GAMMATBL tables to all 768 RGB bytes once per palette change, then sends the corrected RGB888 palette to VGA or converts it to LCD wire-order RGB565. Invalid gamma values fall back to table 0. The WAD palette remains unchanged. As with the engine tables, level 0 uses the first Doom gamma table rather than a newly invented curve.

The serial diagnostic starts with DOOM_COLOR:C5 and reports the selected PLAYPAL and gamma. LCD profile selection, RGB332 pin mapping, VGA timing, TV fit, and the C4 direct color test are preserved.

## Verification scope

Regression tests compile the actual palette-selection function and actual I_SetPalette. They cover menu open/close with damage, pickup, radiation and berserk; stable-menu caching; truecolor redraw signaling; all five gamma tables with literal RGB and RGB565/endian fixtures; palette selection; invalid gamma; and unchanged WAD bytes. Existing LCD/VGA and installer tests remain part of the build.

These fixes address confirmed Doom behavior. They do not establish the cause of every color fault in the user's TV photos. Physical TV output still requires device verification.

Install both OS and Doom using the new installer with the matching ILI9488 or ILI9341 profile. An OS-only Freslest Update ZIP cannot replace the Doom partition.
