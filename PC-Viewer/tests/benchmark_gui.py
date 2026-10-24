"""Benchmark the actual Tk event loop under a finite burst of complete frames."""
from pathlib import Path
import sys,time,threading,json,tkinter as tk
p=Path(sys.argv[1]).resolve();sys.path.insert(0,str(p));import freslest_viewer as v
root=tk.Tk();app=v.Viewer(root);root.geometry('1000x800');root.update_idletasks()
for item in root.tk.splitlist(root.tk.call('after','info')):root.after_cancel(item)
app.connected=True;app.zoom.set('3x');stop=threading.Event();beats=[];presented=[0];real=app.show_image
frame=(480,320,b'P6\n480 320\n255\n'+b'\x20\x40\x60'*(480*320),1)
def draw(*a,**kw):presented[0]+=1;return real(*a,**kw)
app.show_image=draw
started=time.perf_counter()
def producer():
 while time.perf_counter()-started<1.1 and not stop.is_set():app.post('frame',frame);stop.wait(.001)
def heartbeat():
 beats.append(time.perf_counter());root.after(10,heartbeat)
thread=threading.Thread(target=producer,daemon=True);thread.start();heartbeat();root.after(1,app.pump);root.after(1400,app.close)
try:root.mainloop()
finally:stop.set();thread.join(1)
gaps=[b-a for a,b in zip(beats,beats[1:])]
print(json.dumps({'client':str(p),'presented':presented[0],'heartbeat_samples':len(beats),'worst_event_loop_gap_ms':round(max(gaps,default=0)*1000,2)}))
