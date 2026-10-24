"""Real pyserial over a POSIX pseudo terminal, exercising the PC receiver."""
from pathlib import Path
import importlib.util, os, pty, struct, sys, threading, time
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'PC-Viewer'))
from protocol import HEADER,MAGIC,checksum
spec=importlib.util.spec_from_file_location('viewer',r/'PC-Viewer/freslest_viewer.py');v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)
master,slave=pty.openpty();seen=[];stop=threading.Event();peer_done=threading.Event();errors=[]
payload=b'\xff\xff'*(480*320//128)
packet=HEADER.pack(MAGIC,len(payload),77,480,320,checksum(payload))+payload
os.set_blocking(master,False)
def peer():
 try:
  buffer=b'';deadline=time.monotonic()+6
  while time.monotonic()<deadline:
   try:buffer+=os.read(master,4096)
   except BlockingIOError:time.sleep(.001);continue
   while b'\n' in buffer:
    line,buffer=buffer.split(b'\n',1);seen.append(line)
    if line==b'FS?':os.write(master,b'FSHELLO 2 UART 480 320\n')
    elif line.startswith(b'FS1 '):os.write(master,b'Freslest OS 6.14\r\nFSREADY 921600\r\n')
    elif line==b'FS2':
     for i in range(0,len(packet),103):os.write(master,packet[i:i+103]);time.sleep(.001)
    elif line==b'FS0':peer_done.set();return
  raise RuntimeError('Serial peer timed out')
 except BaseException as e:errors.append(e)
t=threading.Thread(target=peer,daemon=True);t.start()
viewer=v.Viewer.__new__(v.Viewer);events=[]
def post(kind,value):
 events.append((kind,value))
 if kind=='frame':stop.set()
viewer.post=post;viewer.controls=v.Controls()
try:
 viewer.receive(os.ttyname(slave),921600,1,stop);t.join(2)
 assert not errors and peer_done.is_set(),errors
 frames=[e[1] for e in events if e[0]=='frame'];assert len(frames)==1 and frames[0][0:2]==(480,320) and frames[0][3]==77
 assert frames[0][2].endswith(b'\xff\xff\xff'*480*320)
 assert seen[0:2]==[b'FS?',b'FS1 921600 1'] and b'FS2' in seen and seen[-1]==b'FS0' and not any(k=='error' for k,val in events),events
 print('PASS: real pyserial connection/fragmented byte reads, baud negotiation, full image reception and FS0 shutdown over a pseudo terminal. Physical Windows USB bridge is not simulated.')
finally:os.close(master);os.close(slave)
