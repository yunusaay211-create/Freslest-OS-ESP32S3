# OS and Doom update discovery with display preservation

The prior VGA-only package was tagged for the ILI9341 board identity. The
ILI9488 parser rejects that identity and omits the ZIP from its list. The prior
new parser also always preferred Doom, preventing OS selection from a dual ZIP.

Keep ILI9488/ILI9341 controller selection, saved LCD/VGA output, VGA640x480 at
25.175MHz, OS menu sizing, GPIOs, apps and audio/video source unchanged. The
user chose option1: only Update source changes, with linked OS images rebuilt;
Doom and the Windows executable runtime are reused.

Add explicit target inspection and an output-aware variant. The updater scans
each ZIP for OS and Doom independently and reinspects the exact selected target
and current output before validating/staging or erasing. Existing OS A/B and
single-slot Doom write boundaries remain. Doom never mutates the OS journal,
previous-OS pointer or boot selection. Output lookup is read-only after gfx
initialization; package selection never writes display settings.

Distribution contains two matching LCD-default OS bootstrap ZIPs and a separate
VGA Doom ZIP. Old readers select their matching OS identity. New readers select
the matching OS and the correct Doom for the current output. Generic bootstrap
OS defaults retain an existing saved VGA choice without imposing VGA on LCD
users. Full fresh VGA installation retains its dedicated VGA-preset OS.

Exactly seven Windows archive entries change: three OS images, three checksum
manifests and the version report. All three Doom images, Windows runtime/PYZ,
other firmware segments and112 audio tracks remain byte-identical.

Verify current and frozen old C parsers for both LCD models, real discovery
and writes in both output modes, selected-target replacement, integrity errors,
both OS slots and every existing power-cut case. Validate actual cross-built
images against partition limits and the real three-package collection before
delivery. Physical device operation remains a separate observation.
