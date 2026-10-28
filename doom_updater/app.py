"""Dedicated Doom VGA update. It never packages or writes an OS image."""
import os,sys
for name in ('stdout','stderr'):
 if getattr(sys,name) is None:setattr(sys,name,open(os.devnull,'w',encoding='utf-8'))
import contextlib,queue,threading,tkinter as tk
from tkinter import ttk
from serial.tools import list_ports
from flash_doom import flash_doom
class DoomUpdater(tk.Tk):
 def __init__(self):
  super().__init__();self.title('Freslest Doom VGA | 1.1');self.geometry('640x480');self.minsize(600,440)
  self.busy=False;self.events=queue.Queue();self.port=tk.StringVar();self.baud=tk.StringVar(value='460800')
  box=ttk.Frame(self,padding=24);box.pack(fill='both',expand=True)
  ttk.Label(box,text='DOOM VGA',font=('Segoe UI',24,'bold')).pack(anchor='w')
  ttk.Label(box,text='Yalnızca Doom güncellemesi',font=('Segoe UI',13)).pack(anchor='w',pady=(4,14))
  ttk.Label(box,text='OS, kayıtlı ekran ayarları, oyun verileri ve kayıtlar korunur.\nDoom: 320×240 • VGA: 640×480 / 25,175 MHz',wraplength=570).pack(anchor='w',pady=(0,15))
  row=ttk.Frame(box);row.pack(fill='x')
  self.ports=ttk.Combobox(row,textvariable=self.port,state='readonly',width=20);self.ports.pack(side='left')
  self.refresh_button=ttk.Button(row,text='Portları yenile',command=self.refresh);self.refresh_button.pack(side='left',padx=8)
  ttk.Combobox(row,textvariable=self.baud,values=('460800','115200','921600'),state='readonly',width=10).pack(side='right')
  self.button=ttk.Button(box,text='Doom’u güncelle',command=self.start);self.button.pack(anchor='e',pady=14)
  self.status=tk.StringVar(value='ESP32-S3 programlama USB’sini bağla ve portu seç.')
  ttk.Label(box,textvariable=self.status,wraplength=560).pack(anchor='w',pady=(0,8))
  self.progress=ttk.Progressbar(box,mode='indeterminate');self.progress.pack(fill='x')
  self.log=tk.Text(box,height=9,state='disabled',font=('Consolas',9));self.log.pack(fill='both',expand=True,pady=(10,0))
  self.refresh();self.after(80,self.poll);self.protocol('WM_DELETE_WINDOW',self.close)
 def refresh(self):
  ports=[p.device for p in list_ports.comports()];self.ports['values']=ports
  if len(ports)==1:self.port.set(ports[0])
  elif self.port.get() not in ports:self.port.set('')
 def start(self):
  if self.busy:return
  port=self.port.get().strip()
  if not port:self.status.set('Programlama portunu seç.');return
  self.busy=True;self.button.configure(state='disabled');self.refresh_button.configure(state='disabled');self.ports.configure(state='disabled')
  self.progress.start();self.status.set('Doom güncelleniyor…')
  threading.Thread(target=self.worker,args=(port,int(self.baud.get())),daemon=True).start()
 def worker(self,port,baud):
  events=self.events
  class Output:
   encoding='utf-8'
   def write(self,s):
    if s:events.put(s.replace('\r','\n'))
    return len(s)
   def flush(self):pass
   def isatty(self):return False
  try:
   with contextlib.redirect_stdout(Output()),contextlib.redirect_stderr(Output()):flash_doom(port,baud)
   events.put(('done',True,'Doom güncellendi. Freslest içinden Doom’u aç.'))
  except BaseException as e:events.put(('done',False,str(e)))
 def poll(self):
  try:
   for _ in range(100):
    v=self.events.get_nowait()
    if isinstance(v,tuple):
     self.busy=False;self.progress.stop();self.button.configure(state='normal');self.refresh_button.configure(state='normal');self.ports.configure(state='readonly');self.status.set(v[2]);break
    self.log.configure(state='normal');self.log.insert('end',v);self.log.see('end');self.log.configure(state='disabled')
  except queue.Empty:pass
  self.after(80,self.poll)
 def close(self):
  if not self.busy:self.destroy()
if __name__=='__main__':DoomUpdater().mainloop()
