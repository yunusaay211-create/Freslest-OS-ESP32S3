# Doom private RGB332 transport

User request: repair Doom VGA colors and demo resets, and give Doom its own RGB332 path. VGA stays 640×480, target 25,175,000 Hz, 800×525 total timing (~59.94Hz). Existing GPIO wiring and OS 480×320 menus remain unchanged. OS/shared production sources must not change.

Current code already owns a Doom panel, but uses the same byte-packed converter module as the shared backend. The new Doom module owns its PLAYPAL-to-RGB332 cache and writes one RGB332 value into each zero-extended 16-bit DMA word. Only LCD D0..D7 are wired to the existing eight GPIOs; D8..D15 are explicitly disabled. This keeps RGB332's 256 colors and eight physical data lines. The DMA word is not interpreted as RGB565; conversion/swizzling stays disabled. Each peripheral clock still represents one VGA pixel.

Approaches: keeping the old 8-bit byte stream would preserve the current transport; changing channel gains would hide a symptom without evidence. Builder choice for the user's dedicated-path request: a private word-based RGB332 transport, while retaining timing, pin order, ownership callbacks and fit setting. This changes the transfer contract rather than tuning the palette. Its effect on the photographed colors requires device testing; the investigation has not established a palette error or electrical fault.

Two PSRAM buffers grow from 307,200 to 614,400 bytes each. The two internal 20-line bounce buffers grow to 25,600 bytes each. Generic Doom progress screens expand their byte output backward in the same DMA buffer; normal Doom frames render directly into words. The native path does not consume OS RGB565 frames.

Separately, an overdue priority-5 Doom frame could skip every blocking delay and starve the core-1 idle watchdog. A failing 1,000-frame busy-demo test reproduces that scheduling defect. Every presentation now blocks for at least one tick, without disabling watchdogs. Boot logs report reset reason and actual GPIO routes after Doom peripherals initialize.

Validation: all 256 RGB332 combinations, five fit settings and black borders; no writes past either word buffer; cursor updates, palette changes and dropped retries; 8 physical GPIOs with 16-bit DMA stride; unchanged 640×480 timing; old busy-demo test failing and corrected test passing. Build only Doom VGA and replace only its profile payload in the existing complete installer. Physical reset cause and color result remain unconfirmed until a device run/log.
