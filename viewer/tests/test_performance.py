import importlib.util,sys,tracemalloc,unittest
from pathlib import Path
p=Path(sys.argv.pop(1)).resolve() if len(sys.argv)>1 and not sys.argv[1].startswith('-') else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(p));import protocol

class PixelConversion(unittest.TestCase):
 def test_rgb332_colours_and_size_are_preserved(self):
  pixels=bytes(range(256))*600
  data=protocol.ppm(480,320,pixels)
  expected=bytearray(b'P6\n480 320\n255\n')
  for v in pixels:expected.extend(((v>>5)*255//7,((v>>2)&7)*255//7,(v&3)*255//3))
  self.assertEqual(data,expected)
 def test_full_frame_conversion_has_bounded_temporary_memory(self):
  pixels=bytes(range(256))*675
  tracemalloc.start()
  try:
   data=protocol.ppm(480,360,pixels);_,peak=tracemalloc.get_traced_memory()
  finally:tracemalloc.stop()
  self.assertLess(peak,2_500_000,f'one RGB conversion allocated {peak:,} bytes')
  self.assertEqual(len(data),len(b'P6\n480 360\n255\n')+480*360*3)

if __name__=='__main__':unittest.main()
