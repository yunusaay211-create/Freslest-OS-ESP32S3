"""Build checked LCD/VGA native USB packages, retaining legacy OS bootstrap."""
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
PROFILES = {'ili9488': 'ILI9488', 'ili9341': 'ILI9341', 'vga': 'ILI9341'}
PROJECT_MARKERS = {
    OS_PROJECT: (b'FRESLEST_UPDATE:OS_AND_DOOM_V3',),
    DOOM_PROJECT: (b'DOOM_VGA:PRIVATE_320x240', b'DOOM_VGA:NATIVE_RGB332_WORDS',
                   b'DOOM_BOOT reset=', b'no automatic replay'),
}

README = '''Freslest native USB update

TURKCE
ZIP dosyasini ACMADAN FAT16/FAT32 USB bellegin kok dizinine kopyalayin.
Karti programlama/UART portundan besleyin; USB bellegi uygun OTG baglantisi
ve 5V VBUS ile native USB/OTG portuna takin.
Ana Menu > Freslest Update menusunden bu ZIP'i secin. BACK yazmadan once
iptal eder; OK dogrulanmis guncellemeyi baslatir. Gucu kesmeyin.

Eski OS kullaniliyorsa bu ZIP'i ilk kez secmek yalnizca OS'u gunceller.
Kartin yeniden baslamasini, saglikli OS acilisini tamamlamasini ve normal
menuye donmesini bekleyin. Sonra Freslest Update > Doom Update secenegini
acip AYNI ZIP'I secin. Yeni OS'ta OS Update ve Doom Update bagimsizdir;
guncellemek istediginiz hedefi secin. OS guncellemesi ekran ayarini korur.
Doom yazimi yarida kalirsa onayli OS kullanilabilir. OS'u acip AYNI ZIP'I
yeniden uygulayin. Tam PC kurulumu ek bir kurtarma secenegidir.

ENGLISH
Copy this ZIP UNEXTRACTED to the root of a FAT16/FAT32 USB flash drive.
Power the board through its programming/UART port. Connect the drive to
the native USB/OTG port with a suitable OTG connection and 5V VBUS.
Open Main Menu > Freslest Update and select this ZIP. BACK cancels before
writing; OK starts the verified update. Keep power connected.

On an older OS, selecting this ZIP first updates OS only. Let the board
reboot, finish healthy OS startup, and return to the normal menu. Then open
Freslest Update > Doom Update and select this SAME ZIP to update Doom.
On the updated OS, OS Update and Doom Update are independent: choose the
target you intend to update. Updating OS preserves the display setting.

The Doom update replaces only its dedicated 0xE0000 application slot.
WAD, filesystem, NVS settings and saves are retained. A Doom write does
not change the selected OS boot slot. If power is lost while writing Doom,
the confirmed OS remains available: start OS and apply this same ZIP again.
A complete PC installer is an additional recovery option. SHA256 detects
corruption; it does not authenticate the publisher. Install packages you trust.
'''


def _tokens(image, prefix):
    """Collect whole firmware string tokens, so similar model names cannot match."""
    tokens = set()
    offset = 0
    while True:
        start = image.find(prefix, offset)
        if start < 0:
            return tokens
        end = image.find(b'\0', start)
        tokens.add(image[start:] if end < 0 else image[start:end])
        offset = start + len(prefix)


def checked_image(path, project, limit, profile='vga', require_native_doom=True):
    """Validate a complete ESP image; the original three-argument API stays VGA.

    Only an explicit LCD reuse request may omit the newer Doom build markers.
    Project, chip, flash, slot, LCD model, checksum and SHA checks always apply.
    """
    if project not in PROJECT_MARKERS:
        raise ValueError('Unknown Freslest firmware project: ' + str(project))
    if profile not in PROFILES:
        raise ValueError('Unknown Freslest display profile: ' + str(profile))
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
    expected_lcd = b'FRESLEST_LCD:' + PROFILES[profile].encode('ascii')
    if _tokens(image, b'FRESLEST_LCD:') != {expected_lcd}:
        raise ValueError(f'{project}: expected only the exact {expected_lcd.decode("ascii")} firmware marker')
    video = _tokens(image, b'FRESLEST_VIDEO:VGA')
    expected_video = {COMMON_MARKERS[0]} if profile == 'vga' else set()
    if video != expected_video:
        raise ValueError(f'{project}: wrong display profile or VGA preset; expected {profile}')
    markers = PROJECT_MARKERS[project] if (project == OS_PROJECT or profile == 'vga' or require_native_doom) else ()
    for marker in markers:
        if marker not in image:
            raise ValueError(f'{project}: missing firmware marker {marker.decode("ascii")}')
    return image


def _manifest(target, image, profile='vga', explicit_display=False):
    value = {
        'format': 'freslest-usb-update',
        'format_version': 3 if target == 'doom' else 2,
        'updater_min': 3 if target == 'doom' else 2,
        'board': 'esp32s3-n16r8-' + PROFILES[profile].lower() + '-buttons',
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
    if profile != 'vga' or explicit_display:
        value['display'] = ('vga' if profile == 'vga' else 'lcd') if target == 'doom' else 'preserve'
    return (json.dumps(value, indent=2) + '\n').encode('utf-8')


def _readme(profile, doom_only=False, reuse_doom=False):
    title = 'Freslest ' + ('VGA 25.175 MHz' if profile == 'vga' else PROFILES[profile])
    if doom_only:
        body = '''Doom native USB update / Doom USB guncellemesi

ENGLISH
This package requires the updated OS with independent OS Update and Doom
Update targets. Bootstrap an older OS with the matching OS + Doom package
first, and let it reboot into a healthy OS before continuing.
Copy this ZIP UNEXTRACTED to the root of a FAT16/FAT32 USB drive. Connect
the drive to the native USB/OTG port with 5V VBUS and power the board from
the programming/UART port. Open Freslest Update > Doom Update and select
this ZIP. Check the target, then press OK. Keep power connected.
This package replaces only the Doom application slot. WAD, filesystem,
NVS settings, saves and the selected OS boot slot are retained.

TURKCE
Bu paket yeni OS'taki bagimsiz Doom Update hedefini gerektirir. Eski OS'u
once uygun OS + Doom paketiyle guncelleyin ve normal acilisin bitmesini
bekleyin. Bu ZIP'i ACMADAN FAT16/FAT32 USB bellegin kok dizinine kopyalayin.
Native USB/OTG portunda 5V VBUS kullanin; karti programlama/UART portundan
besleyin. Freslest Update > Doom Update icinde bu ZIP'i secin ve OK'a basin.
Gucu kesmeyin. WAD, dosya sistemi, NVS ayarlari ve kayitlar korunur.

SHA256 detects corruption; it does not authenticate the publisher.
Install packages you trust. Physical hardware validation is still required.
'''
    else:
        body = README
    if profile == 'vga':
        body += '\nUse the VGA display setting for this Doom image. / Doom icin VGA ekran ayarini kullanin.\n'
    elif reuse_doom:
        body += '\nThe LCD Doom game is the preserved release image. / LCD Doom oyunu korunan surum imajidir.\n'
    return title + '\n\n' + body


def _write_package(output, entries):
    """Publish only a complete, verified archive, preserving any prior output."""
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=output.name + '.', suffix='.tmp', dir=output.parent, delete=False) as staged:
        temporary = Path(staged.name)
    try:
        with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as package:
            for name, data in entries:
                package.writestr(name, data)
        with zipfile.ZipFile(temporary) as package:
            if package.testzip() is not None:
                raise ValueError('ZIP verification failed')
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    return output


def make_package(os_path, doom_path, output, profile='vga', reuse_doom=False):
    """Validate both firmwares, then atomically write a dual-manifest ZIP."""
    os_image = checked_image(os_path, OS_PROJECT, OS_SLOT_SIZE, profile=profile)
    doom_image = checked_image(doom_path, DOOM_PROJECT, DOOM_SLOT_SIZE, profile=profile,
                               require_native_doom=not reuse_doom)
    return _write_package(output, [
        ('freslest-update.json', _manifest('os', os_image, profile)),
        ('freslest-doom-update.json', _manifest('doom', doom_image, profile)),
        ('firmware/freslest_os_s3.bin', os_image),
        ('firmware/freslest_doom_s3.bin', doom_image),
        ('README.txt', _readme(profile, reuse_doom=reuse_doom)),
    ])


def make_doom_package(doom_path, output, profile='vga', reuse_doom=False):
    """Build a Doom-only package with explicit display routing and no OS image."""
    doom_image = checked_image(doom_path, DOOM_PROJECT, DOOM_SLOT_SIZE, profile=profile,
                               require_native_doom=not reuse_doom)
    return _write_package(output, [
        ('freslest-doom-update.json', _manifest('doom', doom_image, profile, explicit_display=True)),
        ('firmware/freslest_doom_s3.bin', doom_image),
        ('README.txt', _readme(profile, doom_only=True, reuse_doom=reuse_doom)),
    ])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--os', type=Path, help='Built OS image with the native OS/Doom updater')
    parser.add_argument('--doom', type=Path, required=True, help='Built private RGB332 Doom image')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=tuple(PROFILES), default='vga')
    parser.add_argument('--reuse-doom', action='store_true', help='Explicitly reuse a released LCD Doom image')
    parser.add_argument('--doom-only', action='store_true', help='Package Doom only; --os is not required')
    args = parser.parse_args()
    if not args.doom_only and args.os is None:
        parser.error('--os is required unless --doom-only is selected')
    try:
        if args.doom_only:
            print(make_doom_package(args.doom, args.output, args.profile, args.reuse_doom))
        else:
            print(make_package(args.os, args.doom, args.output, args.profile, args.reuse_doom))
    except (OSError, ValueError) as error:
        parser.exit(2, f'error: {error}\n')
