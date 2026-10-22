import sys,threading,types,unittest
from pathlib import Path
from unittest import mock
p=Path(sys.argv.pop(1)).resolve() if len(sys.argv)>1 and not sys.argv[1].startswith('-') else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(p))
import freslest_viewer as v
from protocol import HEADER,MAGIC,checksum

class Clock:
 def __init__(self):self.now=100.
 def __call__(self):return self.now

class Peer:
 def __init__(self,clock,**kw):
  self.clock=clock;self.baudrate=kw['baudrate'];self.timeout=kw['timeout'];self.is_open=False;self.rx=bytearray();self.commands=[]
 @property
 def in_waiting(self):return len(self.rx)
 def open(self):self.is_open=True
 def close(self):self.is_open=False
 def reset_input_buffer(self):self.rx.clear()
 def write(self,data):
  self.commands.append(data)
  if data==b'FS?\n':self.rx+=b'FSHELLO 2 NATIVE 480 320\n'
  elif data.startswith(b'FS1 '):self.rx+=b'FSREADY 921600\n'
  elif data==b'FS2\n':
   payload=b'\xff\xff'*300
   self.rx+=HEADER.pack(MAGIC,len(payload),7,240,160,checksum(payload))+payload
  return len(data)
 def read(self,n):
  # pyserial waits for n bytes or timeout, even if a short frame is complete.
  if len(self.rx)<n:self.clock.now+=self.timeout
  data=bytes(self.rx[:n]);del self.rx[:len(data)];return data
 def flush(self):self.clock.now+=10. # Model a stalled USB output queue.

class SerialResponsiveness(unittest.TestCase):
 def test_complete_short_frame_does_not_wait_for_an_8192_byte_read(self):
  clock=Clock();peer=Peer(clock,baudrate=115200,timeout=.025);stop=threading.Event()
  viewer=v.Viewer.__new__(v.Viewer);viewer.controls=v.Controls();frames=[]
  def post(kind,value):
   if kind=='frame':frames.append(value);stop.set()
  viewer.post=post
  with mock.patch.object(v.time,'monotonic',clock):viewer.stream(peer,stop)
  self.assertEqual(frames[0][:2],(240,160))
  self.assertLess(clock.now-100,.020,'complete frame waited for more serial bytes')
 def test_disconnect_does_not_wait_for_unbounded_driver_drain(self):
  clock=Clock();stop=threading.Event();peer=Peer(clock,baudrate=115200,timeout=.025)
  viewer=v.Viewer.__new__(v.Viewer);viewer.controls=v.Controls();events=[]
  def post(kind,value):
   events.append((kind,value))
   if kind=='frame':stop.set()
  viewer.post=post
  info=types.SimpleNamespace(device='COM2',vid=0x303a,description='Freslest PC Viewer')
  with mock.patch.object(v.time,'monotonic',clock),mock.patch.object(v.list_ports,'comports',return_value=[info]),mock.patch.object(v.serial,'Serial',return_value=peer):viewer.receive('COM2',921600,2,stop)
  self.assertTrue(any(k=='frame' for k,_ in events));self.assertFalse(peer.is_open)
  self.assertEqual(peer.commands[-1],b'FSR\nFS0\n')
  self.assertLess(clock.now-100,1.,'disconnect blocked draining the serial driver')

if __name__=='__main__':unittest.main()
