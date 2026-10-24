"""Cross-language wire tests and actual PC serial receiver handshake/cancel tests."""
from pathlib import Path
import importlib.util, os, queue, random, struct, subprocess, sys, tempfile, threading, time, types
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'PC-Viewer'))
from protocol import Decoder, HEADER, MAGIC, MAX_PAYLOAD, checksum, decode_rle, ppm
with tempfile.TemporaryDirectory() as d:
 exe=Path(d)/'codec'
 subprocess.run(['g++','-std=c++17','-fsanitize=address,undefined','-g','-I'+str(root/'shared_components/freslest_video/include'),str(root/'tests/test_screen_codec.cpp'),'-o',str(exe)],check=True)
 wire=subprocess.check_output([str(exe)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
dec=Decoder();frames=[];rng=random.Random(743)
noise=b'Freslest OS 6.14\r\nW (123) wifi: diagnostic\r\nFSREADY 921600\n'
stream=noise+wire
for start in range(0,len(stream),137):frames+=dec.feed(stream[start:start+137])
assert len(frames)==5
for test,(sequence,w,h,pixels) in enumerate(frames[:4]):
 div=2 if test>=2 else 1;assert (sequence,w,h)==(test+1,480//div,(360 if test&1 else 320)//div)
 expected=bytearray()
 for y in range(h):
  for x in range(w):
   i=(y*div)*480+x*div;v=0 if test==0 else ((i*73)^(i>>3)^0x5a5a)&65535
   expected.append(((v>>8)&224)|((v>>6)&28)|((v>>3)&3))
 assert pixels==expected,(test,len(pixels))
assert frames[-1][3][4*480+4]==0
assert frames[-1][3][6*480+5]==255
assert ppm(1,1,b'\xff')==b'P6\n1 1\n255\n\xff\xff\xff'
for run in (1,2,3,127,128):assert decode_rle(bytes((128|run-1,73)),run)==bytes((73,))*run
for bad,count in [(b'\x80',1),(b'\x01\x00',2),(b'\xff\x00',127),(b'',1)]:
 try:decode_rle(bad,count)
 except ValueError:pass
 else:raise AssertionError('Malformed RLE accepted')
# Invalid headers, corrupt payloads, and arbitrary chunks resynchronize to a real frame.
length=struct.unpack_from('<I',wire,8)[0];first=wire[:24+length]
for corrupt in [HEADER.pack(MAGIC,MAX_PAYLOAD+1,1,480,320,0),HEADER.pack(MAGIC,2,1,65535,65535,0),first[:-1]+bytes((first[-1]^1,)),b'\x00'*10000]:
 d=Decoder();got=[];stream=corrupt+first
 for start in range(0,len(stream),31):got+=d.feed(stream[start:start+31])
 assert got and got[-1][0]==1
assert len(Decoder().feed(b'noise'*100000))==0
# Test the real viewer receive() method with a serial peer, not a second viewer implementation.
serial=types.ModuleType('serial');serial.SerialException=OSError
ports=types.ModuleType('serial.tools.list_ports');ports.comports=lambda:[]
tools=types.ModuleType('serial.tools');tools.list_ports=ports
serial.tools=tools
class Peer:
 instances=[]
 def __init__(self,**kwargs):
  self.baudrate=kwargs['baudrate'];self.is_open=False;self.rx=bytearray();self.commands=[];self.dtr=self.rts=None;Peer.instances.append(self)
 def open(self):self.is_open=True;assert self.dtr is False and self.rts is False
 def reset_input_buffer(self):self.rx.clear()
 def write(self,data):
  self.commands.append((self.baudrate,data))
  if data==b'FS?\n':self.rx+=b'FSHELLO 2 UART 480 320\n'
  elif data.startswith(b'FS1 '):
   assert self.baudrate==115200;baud=int(data.split()[1]);self.rx+=f'FSREADY {baud}\r\n'.encode()
  elif data==b'FS2\n':assert self.baudrate in (921600,460800);self.rx+=first
  elif data not in (b'FSR\n',b'FSR\nFS0\n'):raise AssertionError(data)
 def read(self,n):
  data=bytes(self.rx[:min(n,73)]);del self.rx[:len(data)]
  if not data:time.sleep(.001)
  return data
 def flush(self):pass
 def close(self):self.is_open=False
serial.Serial=Peer
sys.modules.update({'serial':serial,'serial.tools':tools,'serial.tools.list_ports':ports})
spec=importlib.util.spec_from_file_location('viewer',root/'PC-Viewer/freslest_viewer.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
for baud,div in [(921600,1),(460800,2)]:
 viewer=module.Viewer.__new__(module.Viewer);events=[];stop=threading.Event()
 def post(kind,value):
  events.append((kind,value))
  if kind=='frame':stop.set()
 viewer.post=post;viewer.controls=module.Controls()
 viewer.receive('COM5',baud,div,stop)
 assert any(k=='frame' for k,v in events) and not any(k=='error' for k,v in events)
 assert Peer.instances[-1].commands[0]==(115200,b'FS?\n')
 assert Peer.instances[-1].commands[1]==(115200,f'FS1 {baud} {div}\n'.encode())
 assert Peer.instances[-1].commands[-1]==(baud,b'FSR\nFS0\n') and not Peer.instances[-1].is_open
assert len(events)>=2
print('PASS: production C++ RGB565 -> RGB332/RLE -> actual Python decoder, 5 full/half frames, all 65536 input colors, pointer, malformed packets/noise/CRC, actual viewer serial handshake/frame/cancel at both baud rates.')
