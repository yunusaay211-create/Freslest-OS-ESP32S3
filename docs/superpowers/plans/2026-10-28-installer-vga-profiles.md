# Installer VGA profiles implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Add source-built VGA25.2MHz to the release1.1 installer's Display step, retaining both LCDs.

**Architecture:** Three independently verified seven-file payloads: ili9488,ili9341,vga. VGA profiles compile ILI9341 as LCD recovery fallback, select VGA on first profile activation and preserve later output choices. The same app rendering and RGB332 pins/porches remain.

**Tech Stack:** Python/Tk, ESP-IDF5.4.4, C/C++, GitHub Actions Windows builds.

**Spec:** User's explicit installerVGA request, corrected to25.2MHz and the interfaces below. Implementation authorized by the user; proceed without a new approval step.

## Global constraints

- Release1.1 and C5 Doom menu/gamma fixes stay intact. Reported TV red tint remains unresolved.
- FRESLEST_VGA_PROFILE CMake cache string: lcd,vga; explicitly pass each build to prevent cached-profile leakage.
- vga: LCD9341 fallback, PCLK25200000, marker FRESLEST_VIDEO:VGA252_25200000. All new VGA output uses25.2MHz.
- User corrected16MHz to25.2MHz. Existing800×525 timings give640×480at60Hz at25.2MHz; physical output is unverified.
- BACK+RESET always permitsLCD recovery. No pinmap or resistor changes.

## Review focus

- A valid-hash wrong-profile binary must never be flashed.
- Old persisted LCD selection must not suppress first activation of a new VGA profile; later deliberate LCD selection must persist.
- BACK must win over the preset, including NVS failures.
- Reused CMake build trees must not leak VGA defaults into LCD binaries.
- All three radio rows must fit the existing installer window in both languages.

### Task1: Installer selection and payload identity

Files: installer/payload.py, installer/installer.py, installer/translations.py, tests/test_installer_displays.py.

Interfaces: DISPLAY_MODELS keys ili9488,ili9341,vga; VGA payload identity includes matching VGA marker and FRESLEST_LCD:ILI9341. No fallback for missingVGA. Reject VGA-tagged firmware inLCDprofiles.

- [x] Add failing payload, wrong-profile and realwidget selection tests.
- [x] Add profile rows, identity validation and translated timing/recovery hints.
- [x] Run headless installer tests; native GUI verification occurs inWindowsCI.

### Task2: VGA boot/timing presets

Files: shared_components/freslest_video/CMakeLists.txt, freslest_video.c, tests/vga_stubs/vga_host.h, tests/test_vga_preset.c, tests/test_vga_transition.py.

Interfaces: compile definitions FRESLEST_VGA_PCLK_HZ and FRESLEST_VGA_PRESET_ID(0,252). First activation stores output_v2 and preset252 seenflag in fl_video; subsequent output choices respected. VGA switching from LCD profiles also uses25.2MHz.

- [x] Add failing firstboot/persistence/BACK/clock tests for252.
- [x] Implement validated CMake options, literal profile tags, and bounded NVS preset handling.
- [x] Run all VGA regressions with sanitizers.

### Task3: Source build, package validation and delivery

Files: tools/build_lcd_profiles.py, .github/workflows/windows-exes.yml, tests/test_lcd_builder.py, tests/test_packaged_release_version.py, RELEASE_LCD_VGA.md, VGA_DISPLAY_TR.md.

Interfaces: iterate(profile,LCDmodel,VGAprofile)=(ili9488,9488,lcd),(ili9341,9341,lcd),(vga,9341,vga). Stage each before reconfiguring. Validate tag,OS1.1,size andhash of all21segments. Upload all three directories.

- [x] Update fake compiler fixtures and staging assertions; watch tests fail for two-profile builder.
- [x] Implement three-profile builds and workflow artifact paths; complete tests for the VGA profile.
- [x] Run full headless suite and review integrated diff.
- [x] Push working branch without merge/release publication; build all6apps and nativeWindowsEXE.
- [x] Verify OS1.1, LCD/VGA tags, checksums, all segment/artifact hashes, partition bounds, Windows launches and GUI selection; save and deliver installer.
