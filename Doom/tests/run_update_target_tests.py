"""Run current and frozen legacy production ZIP readers under ASAN/UBSAN.

IDF_PATH supplies the ESP-IDF components/json/cJSON dependency. --bundle also
checks the release ZIP: the native reader must load Doom and the legacy reader OS.
"""
from pathlib import Path
import argparse
import copy
import io
import json
import os
import struct
import subprocess
import tempfile
import zipfile
import update_target_fixtures as fixtures

from update_target_fixtures import DOOM_PATH, DOOM_PROJECT, OS_PATH, OS_PROJECT, archive, esp_image, manifest

ROOT = Path(__file__).resolve().parents[2]
LEGACY_COMMIT = '8d599ced5d172e76c5dc6c972498eea3182422d1'
ENV = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:abort_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1'}


def compile_probes(build, lcd_model=9341, legacy_ref=LEGACY_COMMIT):
    idf = Path(os.environ.get('IDF_PATH', ROOT.parent / 'analysis/host-idf'))
    cjson = idf / 'components/json/cJSON'
    if not (cjson / 'cJSON.c').is_file():
        raise SystemExit('Set IDF_PATH to ESP-IDF (components/json/cJSON/cJSON.c is required)')
    legacy = build / 'legacy'
    for relative in ('common/usb_update/freslest_update_package.c',
                     'common/usb_update/freslest_update_package.h', 'OS/freslest/lcd_profile.h'):
        destination = legacy / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(subprocess.check_output(['git', 'show', legacy_ref + ':' + relative], cwd=ROOT))
    probes = []
    for source_root, name, defines in [(ROOT, 'update_target_probe', []),
                                      (legacy, 'legacy_update_target_probe', ['-DFRESLEST_UPDATE_LEGACY_PROBE'])]:
        executable = build / name
        shared = source_root / 'common/usb_update'
        subprocess.run([
            'gcc', '-std=gnu17', '-Wall', '-Wextra', '-Werror', '-g',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
            '-DFRESLEST_UPDATE_HOST_TEST', '-DFRESLEST_LCD_MODEL=' + str(lcd_model), *defines,
            '-I' + str(shared), '-I' + str(cjson), str(shared / 'freslest_update_package.c'),
            str(ROOT / 'Doom/tests/update_target_probe.c'), str(cjson / 'cJSON.c'),
            '-lcrypto', '-lz', '-lm', '-o', str(executable),
        ], check=True)
        probes.append(executable)
    return probes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, help='Also load a real dual-manifest release ZIP with both parsers')
    parser.add_argument('--lcd-model', type=int, choices=(9341, 9488), default=9341)
    parser.add_argument('--legacy-ref', default=LEGACY_COMMIT, help='Frozen legacy source ref (default: original public reader)')
    args = parser.parse_args()
    build = ROOT / 'Doom/tests/host_build'
    build.mkdir(exist_ok=True)
    current, legacy = compile_probes(build, args.lcd_model, args.legacy_ref)
    fixtures.BOARD = f'esp32s3-n16r8-ili{args.lcd_model}-buttons'
    os_image = esp_image(OS_PROJECT)
    doom_image = esp_image(DOOM_PROJECT)
    checks = 0
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)

        def check(name, data, target='doom', expected=0, probe=current, option=None):
            nonlocal checks
            path = temporary / (name + '.zip')
            path.write_bytes(data)
            result = subprocess.run([str(probe), str(path), target, *([option] if option else [])],
                                    capture_output=True, text=True, env=ENV)
            if result.returncode != expected:
                raise AssertionError(f'{name}: expected exit {expected}, got {result.returncode}\n{result.stdout}{result.stderr}')
            checks += 1

        check('legacy_os_deflated', archive(os_image=os_image), 'os')
        check('legacy_os_stored', archive(os_image=os_image, compression=zipfile.ZIP_STORED), 'os')
        check('legacy_source_root', archive(os_image=os_image, prefix='Freslest-source/',
              extra=[('source/' + 'a' * 210, b'irrelevant')]), 'os')
        check('doom_deflated', archive(doom_image=doom_image))
        check('doom_stored', archive(doom_image=doom_image, compression=zipfile.ZIP_STORED))
        check('doom_source_root', archive(doom_image=doom_image, prefix='Freslest-source/'))
        dual = archive(os_image=os_image, doom_image=doom_image)
        check('dual_prefers_doom', dual)
        check('dual_explicit_os', dual, 'os', option='--select')
        check('dual_explicit_doom', dual, 'doom', option='--select')
        check('missing_explicit_os', archive(doom_image=doom_image), 'os', 2, option='--select')
        check('missing_explicit_doom', archive(os_image=os_image), 'doom', 2, option='--select')
        check('invalid_requested_target', dual, 'os', 2, option='--invalid-request')
        check('dual_legacy_uses_os', dual, 'os', probe=legacy)
        check('doom_only_legacy_rejected', archive(doom_image=doom_image), 'os', expected=2, probe=legacy)
        check('eocd_comment', archive(doom_image=doom_image, comment=b'PK\x05\x06' * 60))
        for target, image_data in [('os', os_image), ('doom', doom_image)]:
            values = manifest(target, image_data)
            kwargs = {target + '_image': image_data}
            wrong_board = f'esp32s3-n16r8-ili{9488 if args.lcd_model == 9341 else 9341}-buttons'
            for field, value in [('format', 'other'), ('board', wrong_board),
                                 ('layout', 'other'), ('format_version', 2 if target == 'doom' else 3),
                                 ('updater_min', 2 if target == 'doom' else 3), ('version', 'bad\nversion'),
                                 ('target', 'other')]:
                changed = copy.deepcopy(values)
                changed[field] = value
                check(target + '_wrong_' + field, archive(**kwargs, **{target + '_manifest': changed}), target, 2)
            for field, value in [('size', 300.5), ('size', 0), ('file', '../factory.bin'),
                                 ('file', OS_PATH if target == 'doom' else DOOM_PATH), ('sha256', 'z' * 64)]:
                changed = copy.deepcopy(values)
                changed['image'][field] = value
                check(target + '_wrong_image_' + field, archive(**kwargs, **{target + '_manifest': changed}), target, 2)
            duplicate = json.dumps(values)[:-1] + ',"target":"' + target + '","target":"' + target + '"}'
            check(target + '_duplicate_target', archive(**kwargs, **{target + '_manifest': duplicate}), target, 2)
            check(target + '_duplicate_manifest', archive(**kwargs, extra=[(
                  'freslest-doom-update.json' if target == 'doom' else 'freslest-update.json', json.dumps(values))]), target, 2)
            check(target + '_duplicate_image', archive(**kwargs, extra=[(DOOM_PATH if target == 'doom' else OS_PATH, image_data)]), target, 2)
            check(target + '_traversal', archive(**kwargs, prefix='../'), target, 2)
            check(target + '_invalid_enum', archive(**kwargs), target, 3, option='--invalid-target')
            changed = copy.deepcopy(values)
            changed['image']['sha256'] = '0' * 64
            check(target + '_sha', archive(**kwargs, **{target + '_manifest': changed}), target, 3)
            changed = copy.deepcopy(values)
            changed['version'] = '6.99'
            check(target + '_version', archive(**kwargs, **{target + '_manifest': changed}), target, 3)
        explicit_os = manifest('os', os_image)
        explicit_os['target'] = 'os'
        check('explicit_os_target', archive(os_image=os_image, os_manifest=explicit_os), 'os')
        no_target = manifest('doom', doom_image)
        del no_target['target']
        check('doom_requires_target', archive(doom_image=doom_image, doom_manifest=no_target), expected=2)
        for malformed in ['{', '{}', json.dumps(manifest('doom', doom_image)).replace('"doom"', '"os"')]:
            check('malformed_doom_no_fallback', archive(os_image=os_image, doom_image=doom_image,
                  doom_manifest=malformed), expected=2)
        check('duplicate_unused_os_manifest', archive(os_image=os_image, doom_image=doom_image,
              extra=[('another/freslest-update.json', json.dumps(manifest('os', os_image)))]), expected=2)
        check('duplicate_nested_doom_manifest', archive(os_image=os_image, doom_image=doom_image,
              extra=[('another/freslest-doom-update.json', json.dumps(manifest('doom', doom_image)))]), expected=2)
        check('duplicate_long_os_manifest', archive(os_image=os_image, doom_image=doom_image,
              extra=[('a' * 210 + '/freslest-update.json', json.dumps(manifest('os', os_image)))]), expected=2)
        check('duplicate_long_doom_manifest', archive(os_image=os_image, doom_image=doom_image,
              extra=[('a' * 210 + '/freslest-doom-update.json', json.dumps(manifest('doom', doom_image)))]), expected=2)
        check('doom_project_required', archive(doom_image=os_image), expected=3)
        check('os_project_required', archive(os_image=doom_image), 'os', expected=3)
        check('doom_slot_exact', archive(doom_image=esp_image(DOOM_PROJECT, total_size=0xe0000)))
        check('doom_slot_overflow', archive(doom_image=esp_image(DOOM_PROJECT, total_size=0xe0010)), expected=2)
        check('os_slot_exact', archive(os_image=esp_image(OS_PROJECT, total_size=0x1b0000)), 'os')
        check('os_slot_overflow', archive(os_image=esp_image(OS_PROJECT, total_size=0x1b0010)), 'os', expected=2)
        check('deep_json', archive(doom_image=doom_image, doom_manifest=json.dumps(manifest('doom', doom_image))[:-1]
              + ',"extra":' + '[' * 25 + '0' + ']' * 25 + '}'), expected=2)
        check('escaped_nul', archive(doom_image=doom_image, doom_manifest=json.dumps(manifest('doom', doom_image))
              .replace('6.18.1', '6.18.1\\u0000hidden')), expected=2)
        check('unsupported_compression', archive(doom_image=doom_image, compression=zipfile.ZIP_BZIP2), expected=2)
        check('truncated_zip', dual[:-10], expected=2)
        for name, offset, value in [('chip', 12, 0), ('flash', 3, 0x30), ('magic', 0, 0),
                                    ('segments', 1, 17), ('checksum', len(doom_image) - 33, 0),
                                    ('esp_sha', len(doom_image) - 1, doom_image[-1] ^ 1)]:
            damaged = bytearray(doom_image)
            damaged[offset] = value
            check('doom_bad_' + name, archive(doom_image=bytes(damaged)), expected=3)
        damaged = bytearray(doom_image)
        struct.pack_into('<I', damaged, 28, 0xfffffffc)
        check('segment_overflow', archive(doom_image=bytes(damaged)), expected=3)
        damaged = bytearray(archive(doom_image=doom_image, compression=zipfile.ZIP_STORED))
        with zipfile.ZipFile(io.BytesIO(damaged)) as package:
            entry = package.getinfo(DOOM_PATH)
        offset = entry.header_offset + 30 + len(entry.filename.encode()) + len(entry.extra)
        damaged[offset + 300] ^= 0x55
        check('zip_crc_mismatch', bytes(damaged), expected=3)
        if args.bundle:
            actual = args.bundle.resolve().read_bytes()
            check('actual_dual_bundle_native', actual)
            check('actual_dual_bundle_legacy', actual, 'os', probe=legacy)
    print(f'PASS: {checks} native/legacy target ZIP cases under ASAN/UBSAN; SHA/CRC/ESP checks and bounded progress')


if __name__ == '__main__':
    main()
