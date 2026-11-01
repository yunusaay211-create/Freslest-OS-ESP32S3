"""Build one native USB update ZIP readable by both old and current Freslest OS.

Older readers install its format-2 OS image. After a healthy OS restart, the
same ZIP installs Doom through its preferred format-3 manifest.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import tempfile
import zipfile

BOARD = 'esp32s3-n16r8-ili9341-buttons'
LAYOUT = 'freslest-s3-ab-v2'
OS_PROJECT = 'freslest_os_s3'
DOOM_PROJECT = 'freslest_doom_s3'
OS_SLOT_SIZE = 0x1b0000
DOOM_SLOT_SIZE = 0xe0000
COMMON_MARKERS = (b'FRESLEST_VIDEO:VGA25175_25175000', b'FRESLEST_LCD:ILI9341')
PROJECT_MARKERS = {
    OS_PROJECT: (b'FRESLEST_UPDATE:OS_AND_DOOM_V3',),
    DOOM_PROJECT: (b'DOOM_VGA:PRIVATE_320x240', b'DOOM_VGA:NATIVE_RGB332_WORDS',
                   b'DOOM_BOOT reset=', b'no automatic replay'),
}

README = '''Freslest ILI9341 / VGA 25.175 MHz - OS + Doom native USB update

TURKCE
ZIP dosyasini ACMADAN FAT16/FAT32 USB bellegin kok dizinine kopyalayin.
Karti programlama/UART portundan besleyin; USB bellegi uygun OTG baglantisi
ve 5V VBUS ile native USB/OTG portuna takin.
Ana Menu > Freslest Update menusunden bu ZIP'i secin. BACK yazmadan once
iptal eder; OK dogrulanmis guncellemeyi baslatir. Gucu kesmeyin.

Eski OS kullaniliyorsa AYNI ZIP'I IKI KEZ secin. Ilk secim yalnizca OS'u
gunceller. Kartin yeniden baslamasini, saglikli OS acilisini tamamlamasini
ve normal menuye donmesini bekleyin. Ardindan Freslest Update'i yeniden
acin ve AYNI ZIP'I ikinci kez secerek Doom'u guncelleyin.
Yeni OS zaten kuruluysa ayni ZIP'i bir kez secmek Doom'u gunceller.
Doom yazimi yarida kalirsa onayli OS kullanilabilir. OS'u acip AYNI ZIP'I
yeniden uygulayin. Tam PC kurulumu ek bir kurtarma secenegidir.

ENGLISH
Copy this ZIP UNEXTRACTED to the root of a FAT16/FAT32 USB flash drive.
Power the board through its programming/UART port. Connect the drive to
the native USB/OTG port with a suitable OTG connection and 5V VBUS.
Open Main Menu > Freslest Update and select this ZIP. BACK cancels before
writing; OK starts the verified update. Keep power connected.

On an older OS, select this SAME ZIP TWICE. The first selection updates
OS only. Let the board reboot, finish healthy OS startup, and return to
the normal menu. Then reopen Freslest Update and select the SAME ZIP a
second time to update Doom. With the updated OS already installed, one
selection updates Doom.

The Doom update replaces only its dedicated 0xE0000 application slot.
WAD, filesystem, NVS settings and saves are retained. A Doom write does
not change the selected OS boot slot. If power is lost while writing Doom,
the confirmed OS remains available: start OS and apply this same ZIP again.
A complete PC installer is an additional recovery option. SHA256 detects
corruption; it does not authenticate the publisher. Install packages you trust.
'''


def checked_image(path, project, limit):
    """Return validated firmware bytes or raise ValueError for a wrong image."""
    if project not in PROJECT_MARKERS:
        raise ValueError('Unknown Freslest firmware project: ' + str(project))
    image = Path(path).read_bytes()
    if not 288 <= len(image) <= limit:
        raise ValueError(f'{project}: firmware does not fit the {limit:#x} application slot')
    if image[0] != 0xe9 or not 1 <= image[1] <= 16 or struct.unpack_from('<H', image, 12)[0] != 9:
        raise ValueError(f'{project}: expected an ESP32-S3 application image')
    if image[3] >> 4 != 4 or image[23] != 1 or struct.unpack_from('<I', image, 32)[0] != 0xabcd5432:
        raise ValueError(f'{project}: expected 16 MB flash, app descriptor and appended SHA256')
    version_bytes, version_end, _ = image[48:80].partition(b'\0')
    project_bytes, project_end, _ = image[80:112].partition(b'\0')
    try:
        version = version_bytes.decode('ascii')
        actual_project = project_bytes.decode('ascii')
    except UnicodeDecodeError as error:
        raise ValueError(f'{project}: invalid app descriptor text') from error
    if not version_end or not project_end or actual_project != project or not version or any(
            not (character.isalnum() or character in '._-') for character in version):
        raise ValueError(f'{project}: wrong firmware project or invalid version')
    offset, checksum = 24, 0xef
    for segment in range(image[1]):
        if offset + 8 > len(image):
            raise ValueError(f'{project}: truncated ESP segment header')
        length = struct.unpack_from('<I', image, offset + 4)[0]
        offset += 8
        if length > len(image) - offset or (segment == 0 and length < 256):
            raise ValueError(f'{project}: invalid ESP segment length')
        for value in image[offset:offset + length]:
            checksum ^= value
        offset += length
    aligned = (offset + 16) & ~15
    if aligned + 32 != len(image) or image[aligned - 1] != checksum or hashlib.sha256(
            image[:aligned]).digest() != image[aligned:]:
        raise ValueError(f'{project}: ESP image checksum or SHA256 mismatch')
    if b'FRESLEST_LCD:ILI9488' in image or b'FRESLEST_VIDEO:VGA252' in image:
        raise ValueError(f'{project}: wrong LCD profile or obsolete VGA clock')
    for marker in (*COMMON_MARKERS, *PROJECT_MARKERS[project]):
        if marker not in image:
            raise ValueError(f'{project}: missing firmware marker {marker.decode("ascii")}')
    return image


def _manifest(target, image):
    value = {
        'format': 'freslest-usb-update',
        'format_version': 3 if target == 'doom' else 2,
        'updater_min': 3 if target == 'doom' else 2,
        'board': BOARD,
        'layout': LAYOUT,
        'version': image[48:80].split(b'\0', 1)[0].decode('ascii'),
        'image': {
            'file': 'firmware/' + (DOOM_PROJECT if target == 'doom' else OS_PROJECT) + '.bin',
            'size': len(image),
            'sha256': hashlib.sha256(image).hexdigest(),
        },
    }
    if target == 'doom':
        value['target'] = 'doom'
    return (json.dumps(value, indent=2) + '\n').encode('utf-8')


def make_package(os_path, doom_path, output):
    """Validate both firmwares, then atomically write the dual-manifest ZIP."""
    os_image = checked_image(os_path, OS_PROJECT, OS_SLOT_SIZE)
    doom_image = checked_image(doom_path, DOOM_PROJECT, DOOM_SLOT_SIZE)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=output.name + '.', suffix='.tmp', dir=output.parent, delete=False) as staged:
        temporary = Path(staged.name)
    try:
        with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as package:
            package.writestr('freslest-update.json', _manifest('os', os_image))
            package.writestr('freslest-doom-update.json', _manifest('doom', doom_image))
            package.writestr('firmware/freslest_os_s3.bin', os_image)
            package.writestr('firmware/freslest_doom_s3.bin', doom_image)
            package.writestr('README.txt', README)
        with zipfile.ZipFile(temporary) as package:
            if package.testzip() is not None:
                raise ValueError('ZIP verification failed')
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--os', type=Path, required=True, help='Built OS image with the native Doom updater')
    parser.add_argument('--doom', type=Path, required=True, help='Built private RGB332 Doom image')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(make_package(args.os, args.doom, args.output))
    except (OSError, ValueError) as error:
        parser.exit(2, f'error: {error}\n')
