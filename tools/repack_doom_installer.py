"""Replace only VGA Doom and its metadata in the existing full Windows installer."""
from dataclasses import dataclass
from pathlib import Path
import argparse
import hashlib
import json
import struct
import zlib

from check_windows_archive import check_archive

COOKIE = struct.Struct('!8sIIII64s')
TOC = struct.Struct('!iIIIBc')
MAGIC = b'MEI\x0c\x0b\x0a\x0b\x0e'
DOOM = 'firmware\\vga\\freslest_doom_s3.bin'
HASHES = 'firmware\\vga\\SHA256.json'
REPORT = 'firmware\\VERSION_REPORT.json'
ALLOWED = {DOOM, HASHES, REPORT}


@dataclass
class Entry:
    name: str
    packed: bytes
    size: int
    compressed: int
    kind: bytes

    def unpack(self):
        data = zlib.decompress(self.packed) if self.compressed else self.packed
        if len(data) != self.size:
            raise ValueError('Invalid entry size: ' + self.name)
        return data


def read_archive(path):
    data = Path(path).read_bytes()
    if data[:2] != b'MZ' or len(data) < COOKIE.size:
        raise ValueError('Expected a Windows one-file executable')
    magic, size, toc_offset, toc_size, pyver, pylib = COOKIE.unpack(data[-COOKIE.size:])
    start = len(data) - size
    if magic != MAGIC or start < 2 or toc_offset + toc_size != size - COOKIE.size:
        raise ValueError('Invalid archive cookie')
    table = data[start + toc_offset:start + toc_offset + toc_size]
    entries = []
    names = set()
    offset = 0
    while offset < len(table):
        if len(table) - offset < TOC.size:
            raise ValueError('Truncated archive table')
        length, pos, packed_size, raw_size, compressed, kind = TOC.unpack_from(table, offset)
        if length <= TOC.size or offset + length > len(table) or pos + packed_size > toc_offset:
            raise ValueError('Invalid archive entry bounds')
        name = table[offset + TOC.size:offset + length].rstrip(b'\0').decode('utf-8')
        if name in names or compressed not in (0, 1):
            raise ValueError('Invalid or duplicate archive entry: ' + name)
        names.add(name)
        entry = Entry(name, data[start + pos:start + pos + packed_size], raw_size, compressed, kind)
        entry.unpack()
        entries.append(entry)
        offset += length
    return data[:start], pyver, pylib, entries


def write_archive(path, prefix, pyver, pylib, entries):
    body = bytearray()
    table = bytearray()
    for entry in entries:
        name = entry.name.encode('utf-8') + b'\0'
        length = (TOC.size + len(name) + 15) // 16 * 16
        table += TOC.pack(length, len(body), len(entry.packed), entry.size, entry.compressed, entry.kind)
        table += name.ljust(length - TOC.size, b'\0')
        body += entry.packed
    cookie = COOKIE.pack(MAGIC, len(body) + len(table) + COOKIE.size, len(body), len(table), pyver, pylib)
    Path(path).write_bytes(prefix + body + table + cookie)


def repack(base, payload, manifest_path, output):
    if Path(base).resolve() == Path(output).resolve():
        raise ValueError('Output must differ from the base installer')
    check_archive(base, require_sd_audio=True)
    prefix, pyver, pylib, entries = read_archive(base)
    before = {entry.name: entry for entry in entries}
    if not ALLOWED.issubset(before):
        raise ValueError('Base installer lacks the VGA profile')
    binary = Path(payload).read_bytes()
    manifest = json.loads(Path(manifest_path).read_text())
    digest = hashlib.sha256(binary).hexdigest()
    if not (288 <= len(binary) <= 0xE0000 and binary[0] == 0xE9 and struct.unpack_from('<H', binary, 12)[0] == 9):
        raise ValueError('Doom image is not ESP32-S3 or exceeds its partition')
    for marker in (b'DOOM_VGA:PRIVATE_320x240', b'FRESLEST_VIDEO:VGA25175_25175000',
                   b'DOOM_VGA:NATIVE_RGB332_WORDS', b'DOOM_BOOT reset='):
        if marker not in binary:
            raise ValueError('Required Doom image marker is missing')
    if manifest['sha256'] != digest or manifest['bytes'] != len(binary):
        raise ValueError('Doom build manifest does not match the image')
    if manifest['vga_signal'] != [640, 480] or manifest['target_clock_hz'] != 25175000:
        raise ValueError('VGA mode changed')
    hashes = json.loads(before[HASHES].unpack())
    hashes['firmware/freslest_doom_s3.bin'] = digest
    report = json.loads(before[REPORT].unpack())
    report['changes'].extend(['Doom-owned RGB332 palette and raw pixel-word DMA',
                              'Busy Doom demo frames always release the CPU for the idle watchdog'])
    report['profiles']['vga']['freslest_doom_s3.bin'].update(bytes=len(binary), sha256=digest)
    report['doom_vga_build'] = manifest
    report['doom_rgb332'] = {'physical_data_bits': 8, 'dma_word_bits': 16,
                             'upper_byte': 0, 'hardware_verified': False}
    updates = {DOOM: binary, HASHES: (json.dumps(hashes, indent=2) + '\n').encode(),
               REPORT: (json.dumps(report, indent=2) + '\n').encode()}
    changed = []
    for entry in entries:
        if entry.name in updates:
            raw = updates[entry.name]
            changed.append(Entry(entry.name, zlib.compress(raw, 9) if entry.compressed else raw,
                                 len(raw), entry.compressed, entry.kind))
        else:
            changed.append(entry)
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    write_archive(output, prefix, pyver, pylib, changed)
    extraction_paths = check_archive(output, require_sd_audio=True)
    new_prefix, new_pyver, new_pylib, new_entries = read_archive(output)
    if (new_prefix, new_pyver, new_pylib) != (prefix, pyver, pylib):
        raise ValueError('Windows runtime prefix or Python version changed')
    if [e.name for e in new_entries] != [e.name for e in entries]:
        raise ValueError('Archive entry set or order changed')
    after = {e.name: e for e in new_entries}
    for name, entry in before.items():
        if name not in ALLOWED and entry != after[name]:
            raise ValueError('Protected installer entry changed: ' + name)
    for name, raw in updates.items():
        if after[name].unpack() != raw:
            raise ValueError('Replacement differs from the intended bytes: ' + name)
    for profile in ('ili9341', 'ili9488', 'vga'):
        profile_hashes = json.loads(after[f'firmware\\{profile}\\SHA256.json'].unpack())
        for name, expected in profile_hashes.items():
            archive_name = f'firmware\\{profile}\\' + Path(name).name
            if hashlib.sha256(after[archive_name].unpack()).hexdigest() != expected:
                raise ValueError('Firmware hash mismatch: ' + archive_name)
    return {'output': str(Path(output).resolve()), 'bytes': Path(output).stat().st_size,
            'sha256': hashlib.sha256(Path(output).read_bytes()).hexdigest(),
            'changed_entries': sorted(ALLOWED), 'unchanged_entries': len(entries) - len(ALLOWED),
            'windows_extraction_paths': len(extraction_paths), 'sd_audio_tracks': 112,
            'doom': manifest, 'physical_hardware_test': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True)
    parser.add_argument('--payload', required=True)
    parser.add_argument('--manifest', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    print(json.dumps(repack(args.base, args.payload, args.manifest, args.output), indent=2))
