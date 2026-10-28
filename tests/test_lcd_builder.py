import importlib.util
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'installer'))
from payload import validated_payload


class BuilderTests(unittest.TestCase):
    def test_reuses_common_builds_but_preserves_each_staged_model(self):
        spec = importlib.util.spec_from_file_location('lcd_builder', ROOT / 'tools/build_lcd_profiles.py')
        builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(builder)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sdk = root / 'sdk/tools'
            sdk.mkdir(parents=True)
            (sdk / 'idf.py').write_text('# fake compiler for staging orchestration test')
            for name in ('OS', 'Doom', 'firmware'):
                (root / name).mkdir()
            for name in ('doom_flash.wad', 'littlefs.bin'):
                (root / 'firmware' / name).write_bytes(b'preserved data')
            outputs = []
            configs = []

            def fake_compiler(command, cwd, **kwargs):
                output = Path(command[command.index('-B') + 1])
                outputs.append(output)
                model = next(value.split('=')[1] for value in command if value.startswith('-DFRESLEST_LCD_MODEL='))
                preset = next((v.split('=')[1] for v in command if v.startswith('-DFRESLEST_VGA_PROFILE=')), 'lcd')
                configs.append((Path(cwd).name,model,preset))
                marker = ('FRESLEST_LCD:ILI' + model).encode()
                if preset == 'vga':marker += b'FRESLEST_VIDEO:VGA25175_25175000'
                if Path(cwd).name == 'OS':
                    files = ['freslest_os_s3.bin', 'bootloader/bootloader.bin',
                             'partition_table/partition-table.bin', 'ota_data_initial.bin']
                else:
                    files = ['freslest_doom_s3.bin']
                for name in files:
                    file = output / name
                    file.parent.mkdir(parents=True, exist_ok=True)
                    # Staging validates the app descriptor as well as its LCD tag.
                    data = bytearray(80) if name == 'freslest_os_s3.bin' else bytearray()
                    if data:
                        data[0x30:0x50] = b'1.1' + bytes(29)
                    file.write_bytes(data + marker)

            old_path = list(sys.path)
            try:
                with mock.patch.object(builder, 'ROOT', root), mock.patch.dict(builder.os.environ, {'IDF_PATH':str(sdk.parent)}), \
                     mock.patch.dict(sys.modules, {'prepare_firmware':SimpleNamespace(prepare=lambda: None)}), \
                     mock.patch.object(builder.subprocess, 'run', side_effect=fake_compiler):
                    builder.build()
            finally:
                sys.path[:] = old_path
            self.assertEqual(len(outputs), 6)
            self.assertEqual(configs, [(app,str(model),preset) for model,preset in ((9488,"lcd"),(9341,"lcd"),(9341,"vga")) for app in ("OS","Doom")])
            self.assertEqual(len(set(outputs)), 2, 'Common OS/Doom dependencies should be compiled once')
            for model in ('ili9488', 'ili9341', 'vga'):
                segments = validated_payload(root / 'firmware', 'en', model)
                self.assertEqual(len(segments), 7)
                for _, path, _, _ in segments:
                    if path.name in ('freslest_os_s3.bin', 'freslest_doom_s3.bin'):
                        tag = {'ili9488':b'FRESLEST_LCD:ILI9488','ili9341':b'FRESLEST_LCD:ILI9341','vga':b'FRESLEST_VIDEO:VGA25175_25175000'}[model]
                        self.assertIn(tag,path.read_bytes())
                        if path.name == 'freslest_os_s3.bin':
                            self.assertEqual(path.read_bytes()[0x30:0x50], b'1.1' + bytes(29))


if __name__ == '__main__':
    unittest.main()
