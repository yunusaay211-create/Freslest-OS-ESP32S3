import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DiagnosticsTests(unittest.TestCase):
    def test_diagnostics_validates_real_flash_components_without_opening_gui(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / 'diagnostics.json'
            result = subprocess.run([sys.executable, str(ROOT / 'installer/installer.py'),
                                     '--diagnostics', str(report)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = json.loads(report.read_text(encoding='utf-8'))
            self.assertTrue(data['ok'])
            self.assertEqual(data['esptool_version'], '4.11.0')
            self.assertEqual(data['pyserial_version'], '3.5')
            self.assertFalse(data['frozen'])
            self.assertGreater(data['stub_text_bytes'], 0)
            self.assertGreater(data['stub_data_bytes'], 0)
            self.assertTrue(data['stub_entry_in_text'])
            self.assertEqual(len(data['firmware']), 7)
            self.assertTrue(all(item['valid'] for item in data['firmware'].values()))


if __name__ == '__main__':
    unittest.main()
