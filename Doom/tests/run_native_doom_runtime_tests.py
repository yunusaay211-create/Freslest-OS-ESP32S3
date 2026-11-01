"""Run the native OS/Doom writer with the real parser/policy under ASan/UBSan.

--packages DIR exercises the three real USB packages in both LCD profiles and
both active outputs, including OS A/B and Doom writes from A/B.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile
import zipfile

from update_target_fixtures import OS_PROJECT, archive, esp_image, manifest

ROOT = Path(__file__).resolve().parents[2]
ENV = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:abort_on_error=1',
       'UBSAN_OPTIONS': 'halt_on_error=1'}


def compile_runtime(lcd_model=9341):
    shared = ROOT / 'common/usb_update'
    cjson = Path(os.environ.get('IDF_PATH', ROOT.parent / 'analysis/host-idf')) / 'components/json/cJSON'
    if not (cjson / 'cJSON.c').is_file():
        raise SystemExit('Set IDF_PATH to ESP-IDF v5.4.4')
    build = ROOT / 'Doom/tests/host_build'
    build.mkdir(exist_ok=True)
    runtime = build / f'test_native_doom_runtime_{lcd_model}'
    subprocess.run([
        'gcc', '-std=gnu17', '-Wall', '-Wextra', '-Werror',
        '-Wno-misleading-indentation', '-Wno-format-truncation',
        '-Wno-unused-variable', '-Wno-deprecated-declarations', '-g',
        '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
        '-DFRESLEST_UPDATE_HOST_TEST', '-DFRESLEST_OTA_HOST_TEST',
        f'-DFRESLEST_LCD_MODEL={lcd_model}',
        '-I' + str(ROOT / 'shared_components/freslest_ota/include'),
        '-I' + str(shared), '-I' + str(ROOT / 'Doom/tests'),
        '-I' + str(ROOT / 'Doom/components/prboom-esp32-compat/include'),
        '-I' + str(ROOT / 'shared_components/freslest_video/include'),
        '-I' + str(cjson), str(shared / 'freslest_update_package.c'),
        str(ROOT / 'Doom/tests/test_native_doom_runtime.c'), str(cjson / 'cJSON.c'),
        '-lcrypto', '-lz', '-lm', '-o', str(runtime),
    ], check=True)
    return runtime


def run_synthetic():
    runtime = compile_runtime()
    doom = esp_image(total_size=131136)
    os_image = esp_image(project=OS_PROJECT, total_size=131136)
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        usb = tmp / 'usb'
        usb.mkdir()
        (tmp / 'doom.bin').write_bytes(doom)
        (tmp / 'os.bin').write_bytes(os_image)
        (usb / 'update.zip').write_bytes(archive(os_image=os_image))
        (usb / 'doom.zip').write_bytes(archive(doom_image=doom))
        os_manifest = manifest('os', os_image)
        os_manifest['display'] = 'preserve'
        doom_manifest = manifest('doom', doom)
        doom_manifest['display'] = 'lcd'
        (usb / 'dual.zip').write_bytes(archive(os_image=os_image, doom_image=doom,
                                            os_manifest=os_manifest, doom_manifest=doom_manifest))
        bad_sha = manifest('doom', doom)
        bad_sha['image']['sha256'] = '0' * 64
        (usb / 'bad-doom-sha.zip').write_bytes(archive(doom_image=doom, doom_manifest=bad_sha))
        (usb / 'wrong-project.zip').write_bytes(archive(doom_image=os_image))
        corrupt = bytearray(doom)
        corrupt[400] ^= 0x55
        (usb / 'corrupt-doom.zip').write_bytes(archive(doom_image=bytes(corrupt)))
        (usb / 'changed-target.zip').write_bytes(archive(os_image=doom))
        changed_display = manifest('doom', doom)
        changed_display['display'] = 'lcd'
        (usb / 'changed-display.zip').write_bytes(archive(doom_image=doom, doom_manifest=changed_display))
        for name, image in (
            ('changed-version.zip', esp_image(version='6.18.2', total_size=131136)),
            ('changed-size.zip', esp_image(total_size=135232)),
            ('changed-sha.zip', esp_image(markers=[b'changed firmware'], total_size=131136)),
        ):
            (usb / name).write_bytes(archive(doom_image=image))
        subprocess.run([str(runtime), str(tmp / 'os.bin'), str(tmp / 'doom.bin')],
                       cwd=tmp, check=True, env=ENV)
        for frame in tmp.glob('native-*.raw'):
            (ROOT / 'Doom/tests/host_build' / frame.name).write_bytes(frame.read_bytes())


def package_image(path, target):
    with zipfile.ZipFile(path) as package:
        suffix = '/firmware/freslest_' + ('os' if target == 'os' else 'doom') + '_s3.bin'
        matches = [name for name in package.namelist() if '/' + name == suffix or name.endswith(suffix)]
        if len(matches) != 1:
            raise AssertionError(f'{path}: expected one {target} image, got {matches}')
        return package.read(matches[0])


def run_packages(packages):
    packages = packages.resolve()
    names = ('UPD88.ZIP', 'UPD41.ZIP', 'DOOMVGA.ZIP')
    for name in names:
        if not (packages / name).is_file():
            raise SystemExit(f'Missing {packages / name}')
    for model in (9341, 9488):
        runtime = compile_runtime(model)
        board_zip = packages / ('UPD41.ZIP' if model == 9341 else 'UPD88.ZIP')
        for vga in (False, True):
            with tempfile.TemporaryDirectory() as directory:
                tmp = Path(directory)
                usb = tmp / 'usb'
                usb.mkdir()
                for name in names:
                    shutil.copyfile(packages / name, usb / name)
                (tmp / 'os.bin').write_bytes(package_image(board_zip, 'os'))
                (tmp / 'doom.bin').write_bytes(package_image(packages / 'DOOMVGA.ZIP' if vga else board_zip, 'doom'))
                subprocess.run([str(runtime), str(tmp / 'os.bin'), str(tmp / 'doom.bin'),
                                '--real-vga' if vga else '--real-lcd'], cwd=tmp, check=True, env=ENV)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packages', type=Path, help='Verify the actual UPD88/UPD41/DOOMVGA USB package set')
    args = parser.parse_args()
    if args.packages:
        run_packages(args.packages)
    else:
        run_synthetic()


if __name__ == '__main__':
    main()
