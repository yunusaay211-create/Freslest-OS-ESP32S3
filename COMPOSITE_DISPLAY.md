# Freslest 6.10: TV alignment and Doom startup

The complete 6.10 installer updates OS and Doom display code. Ordinary
`flash_all.bat COM5` preserves ROMs, Note, NVS and saves; use the actual UART
COM port. The small USB ZIP updates OS only. An older Doom partition retains
its own display driver until the complete package is installed.

## Composite GPIOs in 6.10

This release uses the requested physical ladder order from VIDEO OUT:
**48 -> 39 -> 40 -> 42 -> 2 -> 14 -> 13 -> 8**. It changes bit assignments
for GPIO2, GPIO39 and GPIO42 compared with 6.09. All eight GPIOs remain in use.
OS, Doom and the static AV Lab share this mapping.

| DAC bit | 6.10 GPIO |
|---|---|
| D0, least significant | 8 |
| D1 | 13 |
| D2 | 14 |
| D3 | 2 |
| D4 | 42 |
| D5 | 40 |
| D6 | 39 |
| D7, most significant | 48 |

Physical ladder order starting at VIDEO OUT:
**48, 39, 40, 42, 2, 14, 13, 8**.
RCA center connects to the resistor DAC's VIDEO OUT and shield to common GND.
A bare GPIO is not the analog output. Select the TV's AV/composite input using
the same cable and resistor network that displayed the original source.

The source's nominal ladder has seven 180-ohm series resistors, eight 330-ohm
bit arms and a 360-ohm termination. Its recorded physical build used seven
150-ohm series resistors, eight 330-ohm arms and a 330-ohm termination. The
firmware does not assume these networks have identical measured levels;
keep the already working network for comparison.

ILI9488 MOSI/SCK/CS/DC/RST use GPIO11/12/10/9/8; BL control is GPIO41.
The touchscreen is disabled; its old GPIO14 CS connection is unused by the OS.
GPIO8 is shared across exclusive display modes. Composite starts without
initializing the SPI panel or resetting the LCD, and keeps LCD BL off and CS high.
The existing buttons, DHT11 GPIO38, DFPlayer RX17/TX18, live sound GPIO21 and
native USB GPIO19/20 keep their wiring.

## Select output and recover

Settings > Display selects ILI9488 LCD or Composite TV (4:3). Confirm twice to
save and restart; BACK cancels before saving. Both display drivers use the same
saved output. To recover a blank TV, hold the physical BACK button during RESET
until the LCD starts. This forces LCD without formatting storage.
The Display page shows the current DAC bit order.

## TV alignment

Open Settings > Display > TV alignment. The light theme only changes UI colors;
it does not select a different analog video mode. The TV alignment page works
with either theme. Defaults retain the original centered 100% image.

1. Select Size with UP/DOWN. Reduce it with LEFT, e.g. to 92%.
2. Select Horizontal; RIGHT moves the image right, LEFT moves it left.
3. Select Vertical; RIGHT moves down, LEFT moves up.
4. Press OK Save. BACK cancels the preview and restores the entry settings.

Mouse +/- controls and the Save button also work. Size ranges from 80% to 100%.
Movement is limited to the available margins, keeping the whole image inside
the canvas. At 100% there is no margin for moving it; reduce Size first.
Reset to default previews centered 100%; save it explicitly to commit.
The settings are shared between OS and Doom. On the LCD they do not rescale
the LCD itself; select composite to see TV preview. Sync, burst and frame
period stay fixed. A position adjustment is not an analog signal measurement.

## Doom startup correction

The old startup check treated an asynchronously submitted but not yet displayed
first composite image as a failed boot. New Doom boots could roll back to OS.
Both applications now wait for one presented image and three VSYNC callbacks,
with a ten-second warmup limit. This wait is never accepted as a healthy frame.
Once output is healthy, the normal checks resume, and later sync failure is
reported immediately. The OTA trial timer, 60-frame/ten-second confirmation,
watchdog and rollback are retained.

The OS launch page waits visibly if startup confirmation is still in progress,
then opens Doom. It checks the installed app description and identifies a missing partition or invalid Doom image. UART logs include the failing stage and ESP error. On the existing A/B layout, repair_doom.bat COM5 rewrites only the Doom app; replace COM5 with the actual UART port.
Install the complete 6.10 package for the fix: the small USB ZIP contains no
Doom image. Doom gameplay, mouse gain, buttons and menu behavior are unchanged.

## AV Engine 0.1.1 rendering and timing

6.10 keeps the raw LCD_CAM/GDMA engine introduced in 6.09. Every 910x262 field sample, including
all porches, sync, equalization, serrations, burst and active image, is explicit
in memory. Three aligned PSRAM fields and internal descriptor rings keep the
DMA-visible field immutable. A delayed handover may repeat one old complete
field. The first new-field EOF releases the old field for writing.

The active image is now on physical lines 20..259 of a 262-line progressive
field, with 744 active samples per line. There are no implicit hardware RGB
porch clocks. The output remains 240p-style NTSC-family progressive video, with
alternating burst/active chroma phase. Read AV_ENGINE.md for exact counts.

Hold physical UP during RESET for a static, initially monochrome AV Lab before
normal OS services. UP/DOWN changes scenes and BACK returns after release.
The legacy first-frame health fix, LCD pointer path and identical-image skip
remain. Physical TV compatibility and analog levels still require a device test.

The OS uses a 480x360 logical UI and a uniform 2/3 scale into the full
320x240 RGB332 TV canvas. The footer moves to logical y=336 and the mouse can
reach logical y=359. Fonts and squares retain their aspect ratio. NES Fullscreen
uses 480x360 before the same scale; Doom remains native 320x240. The LCD still
uses 480x320, with its centered 426x320 NES Fullscreen image.

Select 4:3 / Original in the TV's AV picture-format menu. A 16:9 / Zoom TV setting
can stretch or crop any composite image; this firmware does not control it.
Color and text resolution depend on the analog signal and TV. Watchdogs and
OTA rollback remain enabled.

For diagnostics, open the programming/UART monitor at 115200 baud, reset, and
send lowercase i. The log reports output mode, started state, VSYNC count,
image count, duplicate-image count, last-sync age, TV alignment, first-frame
state and the exact GPIO bit order.
These counters show software activity, not a measurement of analog sync voltage.

Software checks and cross builds passed when packaging (see BUILD_REPORT.json).
No physical ESP32/TV/resistor network test was performed here. TV lock, voltage,
color, flicker and game FPS still require a device test.

API reference:
https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html
Input source: supplied Freslest_Composite_Teknik_Kaynak(1).zip,
components/platform/composite_out.c, composite_out.h and video_profile.json.

For the colour controls and labelled bars/grey ramp, see AV_COLOR_FIX.md.
