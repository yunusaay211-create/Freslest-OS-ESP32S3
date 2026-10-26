"""Display-specific firmware verification, independent of GUI/serial dependencies."""
import hashlib
import json
from pathlib import Path
from translations import translate

DISPLAY_MODELS = {'ili9488': 'ILI9488 — 480×320', 'ili9341': 'ILI9341 — 320×240'}
ITEMS = [
    (0x000000, 'bootloader.bin',0x8000),
    (0x008000, 'partition-table.bin',0x1000),
    (0x010000, 'ota_data_initial.bin',0x2000),
    (0x020000, 'freslest_doom_s3.bin',0xE0000),
    (0x100000, 'freslest_os_s3.bin',0x1B0000),
    (0x460000, 'doom_flash.wad',0x950000),
    (0xDB0000, 'littlefs.bin',0x50000),
]

def validated_payload(firmware_root, language='tr', display='ili9488'):
    if display not in DISPLAY_MODELS:
        raise ValueError('Unknown display model: ' + str(display))
    directory=Path(firmware_root)/display
    # The preserved release payload is ILI9488 only. Never use it for ILI9341.
    legacy=display=='ili9488' and not directory.exists()
    if legacy:directory=Path(firmware_root)
    if not (directory/'SHA256.json').is_file():
        raise RuntimeError(translate('missing_display',language,display=DISPLAY_MODELS[display]))
    manifest=json.loads((directory/'SHA256.json').read_text(encoding='utf-8'))
    result=[]
    for address,name,limit in ITEMS:
        path=directory/name;data=path.read_bytes();digest=hashlib.sha256(data).hexdigest()
        if not data or len(data)>limit or digest!=manifest.get('firmware/'+name):
            raise RuntimeError(translate('invalid_file',language,name=name))
        if not legacy and name in ('freslest_os_s3.bin','freslest_doom_s3.bin'):
            if ('FRESLEST_LCD:'+display.upper()).encode() not in data:
                raise RuntimeError(translate('wrong_display',language,display=DISPLAY_MODELS[display],name=name))
        result.append((address,path,len(data),digest))
    return result

