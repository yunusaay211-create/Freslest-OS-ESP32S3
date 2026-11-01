"""Run the native Doom writer with the real parser/policy under ASan/UBSan."""
from pathlib import Path
import os
import subprocess
import tempfile

from update_target_fixtures import OS_PROJECT, archive, esp_image, manifest

root = Path(__file__).resolve().parents[2]
shared = root / 'common/usb_update'
cjson = Path(os.environ.get('IDF_PATH', '')) / 'components/json/cJSON'
if not (cjson / 'cJSON.c').is_file():
    raise SystemExit('Set IDF_PATH to ESP-IDF v5.4.4')
build = root / 'Doom/tests/host_build'
build.mkdir(exist_ok=True)
runtime = build / 'test_native_doom_runtime'
subprocess.run([
    'gcc', '-std=gnu17', '-Wall', '-Wextra', '-Werror',
    '-Wno-misleading-indentation', '-Wno-format-truncation',
    '-Wno-unused-variable', '-Wno-deprecated-declarations', '-g',
    '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
    '-DFRESLEST_UPDATE_HOST_TEST', '-DFRESLEST_OTA_HOST_TEST',
    '-DFRESLEST_LCD_MODEL=9341',
    '-I' + str(root / 'shared_components/freslest_ota/include'),
    '-I' + str(shared), '-I' + str(root / 'Doom/tests'),
    '-I' + str(root / 'Doom/components/prboom-esp32-compat/include'),
    '-I' + str(cjson), str(shared / 'freslest_update_package.c'),
    str(root / 'Doom/tests/test_native_doom_runtime.c'), str(cjson / 'cJSON.c'),
    '-lcrypto', '-lz', '-lm', '-o', str(runtime),
], check=True)

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
    bad_sha = manifest('doom', doom)
    bad_sha['image']['sha256'] = '0' * 64
    (usb / 'bad-doom-sha.zip').write_bytes(archive(doom_image=doom, doom_manifest=bad_sha))
    (usb / 'wrong-project.zip').write_bytes(archive(doom_image=os_image))
    corrupt = bytearray(doom)
    corrupt[400] ^= 0x55
    (usb / 'corrupt-doom.zip').write_bytes(archive(doom_image=bytes(corrupt)))
    (usb / 'changed-target.zip').write_bytes(archive(os_image=doom))
    for name, image in (
        ('changed-version.zip', esp_image(version='6.18.2', total_size=131136)),
        ('changed-size.zip', esp_image(total_size=135232)),
        ('changed-sha.zip', esp_image(markers=[b'changed firmware'], total_size=131136)),
    ):
        (usb / name).write_bytes(archive(doom_image=image))
    subprocess.run([str(runtime), str(tmp / 'os.bin'), str(tmp / 'doom.bin')],
                   cwd=tmp, check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
    for frame in tmp.glob('native-*.raw'):
        (build / frame.name).write_bytes(frame.read_bytes())
