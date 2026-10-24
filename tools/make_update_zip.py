"""Create the USB update ZIP and matching manifest from the built OS image.

No third-party Python modules are needed. The package intentionally updates
only the inactive OS slot; Doom, WAD, LittleFS, NVS settings and saves stay installed.
"""
from pathlib import Path
import argparse,hashlib,json,struct,zipfile

root=Path(__file__).resolve().parents[1]
def make_package(output=None):
 image=(root/'firmware/freslest_os_s3.bin').read_bytes()
 if not 288<=len(image)<=0x1b0000 or image[0]!=0xe9 or struct.unpack_from('<H',image,12)[0]!=9:
  raise ValueError('Expected an ESP32-S3 OS image fitting the 0x1b0000 A/B OS slot')
 if image[3]>>4!=4 or image[23]!=1 or struct.unpack_from('<I',image,32)[0]!=0xabcd5432:
  raise ValueError('Expected a 16 MB image with an app descriptor and SHA256 digest')
 version=image[48:80].split(b'\0',1)[0].decode('ascii')
 project=image[80:112].split(b'\0',1)[0].decode('ascii')
 if project!='freslest_os_s3' or not version or any(not(c.isalnum() or c in '._-') for c in version):
  raise ValueError('The binary is not a versioned Freslest OS image')
 offset=24;checksum=0xef
 for _ in range(image[1]):
  if offset+8>len(image):raise ValueError('Truncated ESP segment')
  length=struct.unpack_from('<I',image,offset+4)[0];offset+=8
  if length>len(image)-offset:raise ValueError('Truncated ESP segment')
  for b in image[offset:offset+length]:checksum^=b
  offset+=length
 aligned=(offset+16)&~15
 if aligned+32!=len(image) or image[aligned-1]!=checksum or hashlib.sha256(image[:aligned]).digest()!=image[aligned:]:
  raise ValueError('ESP image checksum/digest mismatch')
 manifest={'format':'freslest-usb-update','format_version':2,'updater_min':2,
  'board':'esp32s3-n16r8-ili9488-buttons','layout':'freslest-s3-ab-v2','version':version,
  'image':{'file':'firmware/freslest_os_s3.bin','size':len(image),'sha256':hashlib.sha256(image).hexdigest()}}
 encoded=(json.dumps(manifest,indent=2)+'\n').encode('utf-8')
 (root/'freslest-update.json').write_bytes(encoded)
 output=Path(output) if output else root/'updates'/('Freslest-Update-'+version+'.zip')
 output.parent.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as package:
  package.writestr('freslest-update.json',encoded)
  package.writestr('firmware/freslest_os_s3.bin',image)
  package.writestr('README.txt',
   'Freslest '+version+' - ESP32-S3 N16R8 / ILI9488 / physical buttons\n\n'
   'Initial setup: install the complete Freslest 1.0 A/B release with flash_all.bat.\n'
   'Release 1.0 corrects System Info version metadata; application code is preserved.\n'
   'Then copy THIS ZIP UNEXTRACTED to the root of a FAT16/FAT32 USB flash drive.\n'
   'Power the board through its programming/UART port. Connect the drive to\n'
   'the native USB/OTG port using a suitable OTG connection with 5V VBUS.\n'
   'Open Main Menu > Freslest Update (below Game). Select the package.\n'
   'BACK cancels before writing; OK starts the checked update. Keep power on.\n\n'
   'Apps > Files > Disks shows internal C: and attached FAT16/FAT32 USB D:.\n'
   'Browse folders/text, safely eject with FIRE, and reconnect with OK.\n'
   'Use native USB/OTG for storage and UART for power. No automatic format.\n'
   '6.16: Viewer 2.0 Auto native USB/UART, Fit and PC keyboard/mouse.\n'
   'Native: Settings > Display > USB PC Viewer ON, OK, OK to restart.\n'
   'UART: leave Viewer OFF to keep the native USB host for input/storage.\n'
   'Native CDC may appear as COM; the viewer labels NATIVE or UART.\n'
   'PC input is independent of both BLE peers; focus/disconnect releases it.\n'
   'ESP frame/packet use PSRAM; PC uses its own RAM. See NATIVE_USB.md.\n'
   '6.15: RADIO BSS prints linked PSRAM address/size; BLE/Wi-Fi report both pools.\n'
   '6.14 already used PSRAM-first heaps and external static radio BSS.\n'
   'Normal FreeRTOS stacks/controller/DMA buffers still require internal RAM.\n'
   '6.14: internal-RAM startup guards and optional UART USB Screen Viewer.\n'
   'Viewer: programming USB/UART port; PC-Viewer/start_viewer.bat in full package.\n'
   'OS/NES/Recovery screen only; separate Doom firmware remains unchanged.\n'
   '6.13: staged radio boot, offline Recovery, See Log and deep sleep.\n'
   'LEFT+RIGHT held during RESET opens Recovery; three interrupted boots also do.\n'
   'Recovery Wi-Fi/BT switches ONLY save settings. Radios start after Continue.\n'
   'Third action: See Log. Downdate is inert if no verified prior OS exists.\n'
   '5 idle minutes after healthy startup: deep sleep; physical OK/BACK wakes.\n'
   'Updates/PC transfer/USB drive/editor/portal/download activity block sleep.\n'
   'Deep sleep restarts apps; Doom remains unchanged.\n'
   'See RECOVERY_AND_SLEEP.md in the complete package for controls and limits.\n'
   '6.12: standalone Browser downloads pages directly over Wi-Fi. NO PC REQUIRED.\n'
   'No BrowserServer, remote renderer, server address or access key is needed.\n'
   'Supports HTML/text, links, basic GET forms and JPEG images.\n'
   'HTTPS uses verified common CA roots and needs valid clock/NTP.\n'
   'Does not execute JavaScript or play MP4/YouTube video.\n'
   'Full YouTube/ChatGPT web interfaces are unsupported; bookmarks remain.\n'
   'WiFi: device scan/password keyboard or Freslest-Setup at 192.168.4.1.\n'
   'Wi-Fi remains connected after leaving Settings; NTP sets Turkey time.\n'
   'Saved BLE keyboard/mouse peers reconnect on reboot/unexpected disconnect.\n'
   'Wake the peripheral to advertise again; FIRE scans for new devices.\n'
   'Clock/temperature are beside FRESLEST; Wi-Fi/BT beside the battery.\n'
   'DHT11 GPIO38. The absent-battery cross is retained.\n'
   'Doom source and its firmware remain the exact 6.10 package versions.\n'
   'AV Engine 0.1.1 pins/timing and the A/B layout remain unchanged.\n'
   'VIDEO OUT ladder order: 48,39,40,42,2,14,13,8.\n'
   'Composite UI 480x360 -> 320x240 (4:3); LCD 480x320.\n'
   'Hold UP during RESET for AV Lab; BACK during RESET forces LCD.\n'
   'LCD RST is GPIO8. Hold BACK during RESET to force the LCD.\n'
   'Doom gameplay, mouse sensitivity and buttons retain their behavior.\n\n'
   'Only the inactive OS slot is replaced. Doom, WAD, NES games, Note,\n'
   'settings and Doom saves are retained. The new OS must pass its startup\n'
   'check. A failed/interrupted first boot restores the old OS. FIRE in\n'
   'Freslest Update offers a confirmed restore of the previous OS.\n'
   'The first migration from 4.x needs the complete PC installer.\n'
   'New OS sound files are a separate DFPlayer microSD installation.\n'
   'For 6.00, copy MP3/0105.mp3 through MP3/0112.mp3 from the complete\n'
   'project or shared-audio package to the card, replacing the old files.\n'
   'SHA256 checks detect corruption,\n'
   'not who published a ZIP. Install only packages you trust.\n')
 with zipfile.ZipFile(output) as package:
  if package.testzip() is not None:raise ValueError('ZIP verification failed')
 print(output)
 return output

if __name__=='__main__':
 parser=argparse.ArgumentParser(description='Make a compatible Freslest USB update ZIP from firmware/freslest_os_s3.bin')
 parser.add_argument('--output',type=Path)
 make_package(parser.parse_args().output)
