"""Replace only three OS images and their metadata in a verified Windows installer.

The released Doom binaries, Python/Windows runtime, archive order and 112 SD
sounds remain byte-for-byte identical, including each protected compressed blob.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import tempfile
import zlib

from check_windows_archive import check_archive
from make_doom_update_zip import OS_PROJECT, OS_SLOT_SIZE, checked_image
from repack_doom_installer import Entry, read_archive, write_archive

PROFILES = ('ili9488', 'ili9341', 'vga')
REPORT = 'firmware\\VERSION_REPORT.json'
ALLOWED = {REPORT} | {f'firmware\\{profile}\\{name}' for profile in PROFILES
                     for name in ('freslest_os_s3.bin', 'SHA256.json')}


def _unique_object(pairs):
    value = {}
    for name, item in pairs:
        if name in value:
            raise ValueError('Duplicate metadata key: ' + name)
        value[name] = item
    return value


def _json(entry):
    value = json.loads(entry.unpack(), object_pairs_hook=_unique_object)
    if not isinstance(value, dict):
        raise ValueError('Expected a JSON object: ' + entry.name)
    return value


def _encoded(value):
    return (json.dumps(value, indent=2) + '\n').encode('utf-8')


def _verify_payload(entries):
    """Check every firmware file and all 112 audio files against full manifests."""
    hashes = {}
    for profile in PROFILES:
        prefix = f'firmware\\{profile}\\'
        manifest_name = prefix + 'SHA256.json'
        if manifest_name not in entries:
            raise ValueError('Base installer lacks firmware hashes: ' + profile)
        manifest = _json(entries[manifest_name])
        files = {name[len(prefix):]: entry for name, entry in entries.items()
                 if name.startswith(prefix) and name != manifest_name}
        if not {'freslest_os_s3.bin', 'freslest_doom_s3.bin'}.issubset(files):
            raise ValueError('Base installer lacks OS or Doom firmware: ' + profile)
        if any('\\' in name or '/' in name for name in files):
            raise ValueError('Unexpected nested firmware path: ' + profile)
        if set(manifest) != {'firmware/' + name for name in files}:
            raise ValueError('Firmware manifest does not cover every payload: ' + profile)
        for filename, entry in files.items():
            expected = manifest['firmware/' + filename]
            if not isinstance(expected, str) or not re.fullmatch(r'[0-9a-f]{64}', expected) or (
                    hashlib.sha256(entry.unpack()).hexdigest() != expected):
                raise ValueError('Firmware hash mismatch: ' + entry.name)
        hashes[profile] = manifest
    if 'audio_pack\\SHA256.json' not in entries:
        raise ValueError('Installer lacks the SD audio manifest')
    audio = _json(entries['audio_pack\\SHA256.json'])
    if set(audio) != {f'MP3/{number:04}.mp3' for number in range(1, 113)}:
        raise ValueError('SD audio manifest must cover all 112 tracks')
    for filename, expected in audio.items():
        archive_name = 'audio_pack\\' + filename.replace('/', '\\')
        if archive_name not in entries or not isinstance(expected, str) or not re.fullmatch(r'[0-9a-f]{64}', expected) or (
                hashlib.sha256(entries[archive_name].unpack()).hexdigest() != expected):
            raise ValueError('SD audio hash mismatch: ' + archive_name)
    return hashes


def repack(base, profiles, source_commit, output):
    """Validate first, stage a seven-entry replacement, verify it, then publish."""
    base, profiles, output = Path(base), Path(profiles), Path(output)
    if base.resolve() == output.resolve():
        raise ValueError('Output must differ from the base installer')
    if not isinstance(source_commit, str) or not re.fullmatch(r'[0-9a-fA-F]{40}', source_commit):
        raise ValueError('Expected a full 40-character source commit')
    source_commit = source_commit.lower()
    check_archive(base, require_sd_audio=True)
    prefix, pyver, pylib, entries = read_archive(base)
    before = {entry.name: entry for entry in entries}
    if not ALLOWED.issubset(before):
        raise ValueError('Base installer lacks all three OS profiles and metadata')
    hashes = _verify_payload(before)
    report = _json(before[REPORT])
    if not isinstance(report.get('profiles'), dict) or not isinstance(report.get('changes'), list):
        raise ValueError('Base installer has an invalid version report')
    binaries = {}
    for profile in PROFILES:
        metadata = report['profiles'].get(profile)
        if not isinstance(metadata, dict) or not isinstance(metadata.get('freslest_os_s3.bin'), dict):
            raise ValueError('Base version report lacks an OS profile: ' + profile)
        binaries[profile] = checked_image(profiles / profile / 'freslest_os_s3.bin', OS_PROJECT,
                                           OS_SLOT_SIZE, profile=profile)
    updates = {}
    for profile, binary in binaries.items():
        digest = hashlib.sha256(binary).hexdigest()
        hashes[profile]['firmware/freslest_os_s3.bin'] = digest
        report['profiles'][profile]['freslest_os_s3.bin'].update(bytes=len(binary), sha256=digest)
        updates[f'firmware\\{profile}\\freslest_os_s3.bin'] = binary
        updates[f'firmware\\{profile}\\SHA256.json'] = _encoded(hashes[profile])
    change = 'Independent native OS and Doom updates for ILI9488, ILI9341 and VGA; OS preserves the display setting'
    if change not in report['changes']:
        report['changes'].append(change)
    report['native_os_doom_update'] = {
        'source_commit': source_commit, 'format_version': 3, 'updater_min': 3,
        'bootstrap_format_version': 2, 'profiles': list(PROFILES),
        'os_display': 'preserve', 'doom_images': 'unchanged',
        'physical_hardware_test': False,
    }
    updates[REPORT] = _encoded(report)
    changed = []
    for entry in entries:
        if entry.name in updates:
            raw = updates[entry.name]
            changed.append(Entry(entry.name, zlib.compress(raw, 9) if entry.compressed else raw,
                                 len(raw), entry.compressed, entry.kind))
        else:
            changed.append(entry)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(prefix=output.name + '.', suffix='.tmp', dir=output.parent, delete=False) as staged:
        temporary = Path(staged.name)
    try:
        write_archive(temporary, prefix, pyver, pylib, changed)
        extraction_paths = check_archive(temporary, require_sd_audio=True)
        new_prefix, new_pyver, new_pylib, new_entries = read_archive(temporary)
        if (new_prefix, new_pyver, new_pylib) != (prefix, pyver, pylib):
            raise ValueError('Windows runtime prefix or Python version changed')
        if [entry.name for entry in new_entries] != [entry.name for entry in entries]:
            raise ValueError('Archive entry set or order changed')
        after = {entry.name: entry for entry in new_entries}
        for name, entry in before.items():
            if name not in ALLOWED and after[name] != entry:
                raise ValueError('Protected installer entry changed: ' + name)
        for name, raw in updates.items():
            if after[name].unpack() != raw:
                raise ValueError('Replacement differs from intended bytes: ' + name)
        _verify_payload(after)
        summary = {
            'output': str(output.resolve()), 'bytes': temporary.stat().st_size,
            'sha256': hashlib.sha256(temporary.read_bytes()).hexdigest(),
            'source_commit': source_commit, 'changed_entries': sorted(ALLOWED),
            'unchanged_entries': len(entries) - len(ALLOWED),
            'windows_extraction_paths': len(extraction_paths),
            'sd_audio_tracks': 112, 'sd_audio_hashes_verified': 112,
            'doom_images_preserved': list(PROFILES), 'physical_hardware_test': False,
        }
        temporary.replace(output)
        return summary
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--profiles', type=Path, required=True)
    parser.add_argument('--source-commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(repack(args.base, args.profiles, args.source_commit, args.output), indent=2))
    except (OSError, ValueError, KeyError, zlib.error) as error:
        parser.exit(2, f'error: {error}\n')
