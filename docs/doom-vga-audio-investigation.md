# Doom VGA color and OS audio recovery — 2026-10-27

## Color

User reports constant red/warm tint in Doom including its menu, while OS VGA colors look normal. Palette ordering was traced from I_SetPalette through spi_lcd_set_palette to vga_convert: bytes are R,G,B and channel wiring already matches the working core. Saturated red/green/blue endpoint tests pass. No channel swap or red gain change is justified.

An actual converter test reproduced a different, concrete defect: truncation into RGB332 removes two-bit blue from dark neutral colors. Indexed conversion now uses a 4x4 ordered rounding pattern and a bounded 1 KiB palette lookup, keeping each neutral channel mean within three 8-bit levels in the regression. Endpoint primaries, palette changes, RGB565 OS output and LCD/Doom orientation stay covered. This improves low-bit color accuracy; the reported physical red tint still needs device confirmation. It is not proof of a DAC or monitor fault, nor proof of a physical fix.

## Audio

User localized intermittent loss to OS menus/apps. UART1 GPIO18 TX / GPIO17 RX remains exclusive to the DFPlayer; no conflicting new VGA pin was found. Two reproducible protocol defects were found: SD file count used 0x48 (USB count) instead of 0x47, and unexpected SD online/reinsert notifications left ready=true without restoring source, DAC or user volume after module reset. The latter can leave subsequent cues silent until a full reboot.

On explicit SD removal the queue is cleared and playback waits for insertion. On SD insertion/online the nonblocking setup resumes at SD selection, restores DAC/volume and queries the correct SD count, without another reset, stale cue replay, or changing mute settings. No periodic reset or retry storm is introduced for unconnected RX. Diagnostics count recoveries. Host tests inject checksummed online/removal/corrupt frames, check recovery ordering, subsequent cues and mute preservation. This verifies recovery logic, not the electrical cause of the user's intermittent loss; power or wiring still requires device observation if it persists.

Protocol reference (manufacturer library): https://github.com/DFRobot/DFRobotDFPlayerMini/blob/master/DFRobotDFPlayerMini.cpp (parseStack, readFileCounts). Full installer is required to update Doom; OS USB Update ZIP writes only OS slots.


C3 follow-up: generated ordered dots were reproduced on flat source colors, and removed. Current packing matches the supplied working core, without spatial or temporal dithering. Earlier gray-average assertions did not verify source uniformity. See docs/2026-10-27-solid-rgb332.md. Physical red-tint cause remains unproven.
