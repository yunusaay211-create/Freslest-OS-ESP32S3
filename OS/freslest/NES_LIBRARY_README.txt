FRESLEST NES LIBRARY 6.00

See the package root README.md. Enter NES Library to open the native USB
MSC drive. Copy .nes files to its root and Eject on the PC to import.
The 4 MB RAM disk falls back to 2 MB / 1 MB if needed. Persistent ROM/Note
storage remains 320 KiB. Only completely erased storage initializes; existing
files are never formatted on a mount error.
FIRE in the Library opens the existing transfer/result without resetting it.
FIRE inside finished/error details explicitly reconnects a fresh disk.
Eject on the PC imports automatically. If the host omits the Eject command,
press OK Import in details after safely ejecting. OK after import returns.
BACK during an active transfer cancels; BACK after completion keeps the result.
E101-E205 identify the failing storage/memory/USB stage.
