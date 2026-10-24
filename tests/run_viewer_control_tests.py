"""Catch cropped frame edges, mis-scaled pointer coordinates and held input leaks."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'PC-Viewer'))
try:
 from control import geometry,source_point,Controls,control_commands,usage
except ImportError:
 raise AssertionError('Viewer must expose the tested Fit geometry and remote controls')
for w,h in ((480,320),(480,360),(240,160),(240,180)):
 for cw,ch in ((620,300),(960,650),(80,30),(13,13),(1,1)):
  l=geometry(w,h,cw,ch)
  x,y,dw,dh,n,d=l
  assert 0<=x and 0<=y and x+dw<=cw and y+dh<=ch,(w,h,cw,ch,l)
  assert source_point(x,y,l,w,h,1)==(0,0)
  p=source_point(x+dw-1,y+dh-1,l,w,h,2)
  assert p is not None and 0<=p[0]<w*2 and 0<=p[1]<h*2
  assert source_point(x+dw,y,l,w,h,1) is None
assert geometry(480,320,620,300)==(190,70,240,160,1,2)
assert geometry(480,360,620,300,'2x')==(0,0,960,720,2,1)
c=Controls();c.key('Left',True);rev,k,p,b=c.snapshot();assert k[0x50//8]&(1<<(0x50%8))
c.key('Left',True);assert c.snapshot()[0]==rev
c.key('Shift_R',True);assert c.snapshot()[1][0xe5//8]&(1<<(0xe5%8))
c.mouse((50,60),1,True);assert c.snapshot()[2:]==((50,60),1)
cmd=control_commands(c.snapshot());assert b'FSP 50 60 1\n' in cmd and cmd.startswith(b'FSK ')
c.mouse(None,1,False);assert c.snapshot()[3]==0
c.release();assert control_commands(c.snapshot())==b'FSR\n'
assert usage('a')==4 and usage('Up')==0x52 and usage('F12')==0x45
print('PASS: full-frame Fit at all supported sizes, tiny windows, scroll zoom, letterbox rejection, pointer scaling, key edges and focus release')

# Exercise the actual viewer worker against two USB transports with a wire peer.
import importlib.util,threading,types
import serial
from protocol import HEADER,MAGIC,checksum
spec=importlib.util.spec_from_file_location('viewer_controls',r/'PC-Viewer/freslest_viewer.py')
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
ports=[types.SimpleNamespace(device='COM9',vid=None,description='Bluetooth serial'),
 types.SimpleNamespace(device='COM1',vid=0x10c4,description='CP210x USB UART'),
 types.SimpleNamespace(device='COM2',vid=0x303a,description='Freslest PC Viewer'),
 types.SimpleNamespace(device='COM3',vid=0x1a86,description='USB UART')]
v.list_ports.comports=lambda:ports
assert [p.device for p in v.candidates()]==['COM2','COM1','COM3']
payload=b'\xff\xff'*(480*320//128)
packet=HEADER.pack(MAGIC,len(payload),88,480,320,checksum(payload))+payload
class Peer:
 instances=[]
 def __init__(self,**kw):
  self.baudrate=kw['baudrate'];self.rx=bytearray();self.is_open=False;self.writes=[];Peer.instances.append(self)
 def open(self):self.is_open=True
 def close(self):self.is_open=False
 def flush(self):pass
 def reset_input_buffer(self):self.rx.clear()
 def write(self,data):
  self.writes.append((self.baudrate,data))
  if data==b'FS?\n':self.rx+=b'FSHELLO 2 '+(b'NATIVE' if self.port=='COM2' else b'UART')+b' 480 320\n'
  elif data.startswith(b'FS1 '):self.rx+=b'FSREADY 921600\n'
  elif data==b'FS2\n':self.rx+=packet
 def read(self,n):
  data=bytes(self.rx[:min(n,257)]);del self.rx[:len(data)];return data
v.serial.Serial=Peer
for selected,actual,transport,baud in [('Auto','COM2','NATIVE',115200),('COM3','COM3','UART',921600)]:
 viewer=v.Viewer.__new__(v.Viewer);viewer.controls=v.Controls();viewer.controls.key('Left',True);viewer.controls.mouse((50,60),1,True)
 stop=threading.Event();events=[]
 def post(kind,value):
  events.append((kind,value))
  if kind=='frame':stop.set()
 viewer.post=post;viewer.receive(selected,921600,1,stop)
 p=Peer.instances[-1]
 assert p.port==actual and p.baudrate==baud and p.dtr==p.rts==(transport=='NATIVE')
 assert any(k=='connected' and value[1]==transport for k,value in events) and any(k=='frame' for k,value in events)
 controls=b''.join(data for _,data in p.writes)
 assert b'FSK ' in controls and b'FSP 50 60 1\n' in controls
 assert p.writes[-1]==(baud,b'FSR\nFS0\n') and not p.is_open and v.control_commands(viewer.controls.snapshot())==b'FSR\n'
 assert not any(k=='error' for k,value in events),events
print('PASS: Auto excludes Bluetooth COM, selects native signature first; native retains USB baud, UART switches baud; actual receiver sends PC keys/pointer and releases on disconnect')
