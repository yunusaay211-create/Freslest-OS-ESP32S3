"""Check explicit target/display compatibility for both LCD boards and outputs."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile

from update_target_fixtures import OS_PROJECT, archive, esp_image, manifest

ROOT = Path(__file__).resolve().parents[2]
ENV = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:abort_on_error=1',
       'UBSAN_OPTIONS': 'halt_on_error=1'}
MISSING = object()


def compile_probe(model):
    shared = ROOT / 'common/usb_update'
    cjson = Path(os.environ.get('IDF_PATH', ROOT.parent / 'analysis/host-idf')) / 'components/json/cJSON'
    if not (cjson / 'cJSON.c').is_file():
        raise SystemExit('Set IDF_PATH to ESP-IDF v5.4.4')
    build = ROOT / 'Doom/tests/host_build'
    build.mkdir(exist_ok=True)
    probe = build / f'update_output_probe_{model}'
    subprocess.run([
        'gcc', '-std=gnu17', '-Wall', '-Wextra', '-Werror', '-g',
        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
        '-DFRESLEST_UPDATE_HOST_TEST', f'-DFRESLEST_LCD_MODEL={model}',
        '-I' + str(shared), '-I' + str(cjson), str(shared / 'freslest_update_package.c'),
        str(ROOT / 'Doom/tests/update_target_probe.c'), str(cjson / 'cJSON.c'),
        '-lcrypto', '-lz', '-lm', '-o', str(probe),
    ], check=True)
    return probe


def tagged(target, board, display=MISSING, duplicate=False):
    image = esp_image(OS_PROJECT) if target == 'os' else esp_image()
    value = manifest(target, image)
    value['board'] = f'esp32s3-n16r8-ili{board}-buttons' if isinstance(board, int) else board
    if display is not MISSING:
        value['display'] = display
    if duplicate:
        value = json.dumps(value)[:-1] + ',"display":' + json.dumps(display) + '}'
    return archive(**{target + '_image': image, target + '_manifest': value})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--packages', type=Path, help='Also verify the actual three-package USB set')
    args = parser.parse_args()
    checks = 0
    with tempfile.TemporaryDirectory() as directory:
        temporary = Path(directory)
        for model in (9341, 9488):
            probe = compile_probe(model)

            def check(name, data, target, vga, accepted, option=None):
                nonlocal checks
                path = temporary / (name + '.zip')
                path.write_bytes(data)
                command = [str(probe), str(path), target, option or ('--vga' if vga else '--lcd')]
                result = subprocess.run(command, capture_output=True, text=True, env=ENV)
                expected = 0 if accepted else 2
                if result.returncode != expected:
                    raise AssertionError(f'ILI{model} {name}: expected {expected}, got {result.returncode}\n{result.stdout}{result.stderr}')
                checks += 1

            other = 9488 if model == 9341 else 9341
            for vga in (False, True):
                suffix = 'vga' if vga else 'lcd'
                check('os_preserves_' + suffix, tagged('os', model, 'preserve'), 'os', vga, True)
                check('os_wrong_board_' + suffix, tagged('os', other, 'preserve'), 'os', vga, False)
                check('doom_lcd_' + suffix, tagged('doom', model, 'lcd'), 'doom', vga, not vga)
                check('doom_wrong_lcd_board_' + suffix, tagged('doom', other, 'lcd'), 'doom', vga, False)
                check('approved_vga_' + suffix, tagged('doom', 9341, 'vga'), 'doom', vga, vga)
                check('same_board_vga_' + suffix, tagged('doom', model, 'vga'), 'doom', vga, vga)
                check('unapproved_cross_vga_' + suffix, tagged('doom', 9488, 'vga'), 'doom', vga, vga and model == 9488)
                for target in ('os', 'doom'):
                    check(target + '_legacy_same_' + suffix, tagged(target, model), target, vga, True)
                    check(target + '_legacy_cross_' + suffix, tagged(target, other), target, vga, False)
                    check(target + '_unknown_board_' + suffix, tagged(target, 'esp32s3-unknown', 'preserve' if target == 'os' else 'vga'), target, vga, False)
                    valid = 'preserve' if target == 'os' else ('vga' if vga else 'lcd')
                    check(target + '_duplicate_' + suffix, tagged(target, model, valid, True), target, vga, False)
                    invalid_values = [None, True, 0, [], {}, '', 'LCD', 'other']
                    invalid_values += ['lcd', 'vga'] if target == 'os' else ['preserve']
                    for index, display in enumerate(invalid_values):
                        check(f'{target}_invalid_{index}_{suffix}', tagged(target, model, display), target, vga, False)
            # APIs without the actual output argument never use the cross-board exception.
            check('strict_target_board', tagged('doom', other, 'vga'), 'doom', True, False, '--select')
            if args.packages:
                for filename in ('UPD88.ZIP', 'UPD41.ZIP', 'DOOMVGA.ZIP'):
                    data = (args.packages / filename).read_bytes()
                    package_board = 9341 if filename == 'UPD41.ZIP' else 9488
                    for vga in (False, True):
                        for target in ('os', 'doom'):
                            accepted = (target == 'doom' and vga) if filename == 'DOOMVGA.ZIP' else (
                                package_board == model and (target == 'os' or not vga))
                            check(f'actual_{filename}_{target}_{vga}', data, target, vga, accepted)
    print(f'PASS: {checks} target/output/board/display cases under ASAN/UBSAN, including all four board/output configurations')


if __name__ == '__main__':
    main()
