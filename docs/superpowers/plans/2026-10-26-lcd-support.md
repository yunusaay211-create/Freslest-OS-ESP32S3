# LCD support implementation plan

Goal: retain ILI9488 and add ILI9341, selectable at installer step 1.

Architecture: keep the OS's logical 480x320 UI/input/Viewer coordinates; scale only the ILI9341 physical output. Share the controller profile with the separate Doom LCD driver. Build two OS/Doom pairs and select verified payloads in the bilingual installer.

- [x] Add host tests for actual rendering and cursor restoration before implementing the output changes.
- [x] Add model-specific panel dimensions, initialization and RGB565/RGB888 transfers.
- [x] Add installer model choice and tests for missing/swapped profiles and actual flash arguments.
- [x] Add source build and Windows packaging steps that require both profiles.
- [x] Preserve ILI9488 as the default and reject mismatched USB update board IDs.
- [x] Run available host checks and document environment limitations.
- [ ] Run ESP-IDF and Windows builds, then test each physical LCD. These require unavailable build/hardware environments.
