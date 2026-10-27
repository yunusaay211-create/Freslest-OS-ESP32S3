# VGA frame ownership implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Produce an installer and OS update candidates addressing photographed VGA corruption and TV cropping.
**Architecture:** Two PSRAM frames with bounce DMA and explicit frame completion; dedicated raw Doom palette conversion; persisted centered safe area.
**Tech Stack:** Existing C/C++ ESP-IDF5.4.4, sanitizer host tests and Windows CI.
**Spec:** ../specs/2026-10-27-vga-frame-ownership-design.md

## Global constraints
- Preserve existing VGA pins, 640x480 timings and LCD orientation.
- Fit80..100 in5% steps, default90; raw full-size conversion remains available for tests.
- No changes to A/B partitions, audio fixes or Viewer input.
- Build both OS/Doom models; no main merge or release publication.

## Review focus
- Never reuse active/pending frame even on stalled interrupts or failed draw call.
- Flash/cache availability during OTA and NVS with bounce mode.
- Black borders and last source pixel must survive size/model changes.
- Raw Doom palette channels/endpoints and damage palettes must not be overridden.
- Native320 settings bounds and failed NVS save must remain correct.

### Task1: Frame ownership and transport
- [x] Reproduce active scanout modification with failing backend regression.
- [ ] Extend actual backend host test with delayed frame events, timeout, swap, setup cleanup and failure ownership.
- [ ] Implement two frames/bounce configuration and bounded pending-frame handling in shared_components/freslest_video/freslest_video.c; enable XIP and replace unconditional restarts in all four configs.
- [ ] Run tests/test_vga_transition.py and actual firmware builds.

### Task2: Doom RGB332 palette path
- [ ] Add test fixtures for raw RGB888 channels, dark neutrals, palette changes and actual Doom VGA call path before implementation.
- [ ] Add vga_doom palette/renderer, wire I_SetPalette raw palette only in VGA, retain LCD conversion.
- [ ] Run both LCD profile regressions and stack budget.

### Task3: TV safe area
- [ ] Test fit borders/endpoints/cursor, NVS bounds/failure and Display size interaction on both LCD sizes.
- [ ] Implement fit-aware converter, persistent getter/setter and Display size row.
- [ ] Run Linux suite, Windows suite and EXE smoke CI; verify all payload hashes/partition limits, create OS-only ZIPs and update deliverables/provenance.
