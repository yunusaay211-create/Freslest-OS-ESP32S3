"""Freslest USB Viewer for the OS 1.0 release; Screen 2 wire protocol."""
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
try:
    import serial
    from serial.tools import list_ports
except ImportError:
    raise SystemExit('Install pyserial: python -m pip install pyserial==3.5')
from protocol import Decoder, ppm
from control import Controls, control_commands, geometry, source_point

USB_VIDS={0x303a,0x10c4,0x1a86,0x0403}


def candidates():
    ports=[p for p in list_ports.comports() if p.vid in USB_VIDS or 'freslest' in (p.description or '').lower()]
    return sorted(ports,key=lambda p:('freslest' not in (p.description or '').lower(),p.vid!=0x303a,p.device))


def open_device(port,stop):
    info=next((p for p in list_ports.comports() if p.device==port),None)
    c=serial.Serial(port=None,baudrate=115200,timeout=.025,write_timeout=.6)
    c.dtr=c.rts=bool(info and info.vid==0x303a)
    c.port=port
    try:
        c.open();c.reset_input_buffer();c.write(b'FS?\n')
        data=bytearray();deadline=time.monotonic()+.65
        while not stop.is_set() and time.monotonic()<deadline:
            data.extend(c.read(512))
            while b'\n' in data:
                line,_,rest=data.partition(b'\n');data=bytearray(rest)
                words=line.strip().split()
                if len(words)==5 and words[:2]==[b'FSHELLO',b'2'] and words[2] in (b'UART',b'NATIVE'):
                    size=tuple(map(int,words[3:5]))
                    if size in ((480,320),(480,360)):
                        return c,words[2].decode('ascii'),size
            if len(data)>4096:del data[:-512]
        raise RuntimeError('Freslest ekran bağlantısı bulunamadı. OS 1.0 firmware ve USB PC Viewer ayarını kontrol et.')
    except BaseException:
        c.close();raise


class Viewer:
    def __init__(self,root):
        self.root=root;root.title('Freslest USB Viewer | OS 1.0');root.minsize(640,460);root.geometry('960x680')
        self.events=queue.Queue();self.stop=threading.Event();self.thread=None
        self.frame_lock=threading.Lock();self.pending_frame=None
        self._revision=0;self._render_signature=None;self._viewport=None
        self._canvas_image=None;self._placeholder=None;self._scaled_image=None;self._status_at=0
        self.controls=Controls();self.connected=False;self.connection_label='';self.divisor=1
        self.image=self.display_image=None;self.layout=None;self.frame_size=(480,320)
        self.last_frame_at=None
        self.port=tk.StringVar(value='Auto');self.speed=tk.StringVar(value='921600')
        self.quality=tk.StringVar(value='Half');self.zoom=tk.StringVar(value='Fit');self.control_enabled=tk.BooleanVar(value=True)
        self.status=tk.StringVar(value='Searching for Freslest native USB or USB/UART. Close Serial Monitor first.')
        bar=ttk.Frame(root,padding=8);bar.pack(fill='x')
        ttk.Label(bar,text='Port').pack(side='left')
        self.ports=ttk.Combobox(bar,textvariable=self.port,width=15);self.ports.pack(side='left',padx=5)
        ttk.Button(bar,text='Refresh',command=self.refresh).pack(side='left')
        ttk.Label(bar,text='UART baud').pack(side='left',padx=(10,3))
        ttk.Combobox(bar,textvariable=self.speed,values=('921600','460800'),width=8,state='readonly').pack(side='left')
        self.connect_button=ttk.Button(bar,text='Connect / Auto',command=self.connect);self.connect_button.pack(side='left',padx=8)
        ttk.Button(bar,text='Disconnect',command=self.disconnect).pack(side='left')
        options=ttk.Frame(root,padding=(8,0,8,8));options.pack(fill='x')
        ttk.Label(options,text='Resolution').pack(side='left')
        ttk.Combobox(options,textvariable=self.quality,values=('Full','Half'),width=6,state='readonly').pack(side='left',padx=5)
        ttk.Label(options,text='Zoom').pack(side='left',padx=(10,3))
        z=ttk.Combobox(options,textvariable=self.zoom,values=('Fit','1x','2x','3x'),width=5,state='readonly');z.pack(side='left')
        z.bind('<<ComboboxSelected>>',lambda e:self.show_image(reset=True))
        ttk.Checkbutton(options,text='PC keyboard + mouse',variable=self.control_enabled,command=self.release_controls).pack(side='left',padx=8)
        ttk.Button(options,text='Save PNG',command=self.save).pack(side='left')
        area=ttk.Frame(root);area.pack(fill='both',expand=True);area.rowconfigure(0,weight=1);area.columnconfigure(0,weight=1)
        self.canvas=tk.Canvas(area,bg='#11151b',highlightthickness=0,takefocus=True)
        ys=ttk.Scrollbar(area,orient='vertical',command=self.canvas.yview);xs=ttk.Scrollbar(area,orient='horizontal',command=self.canvas.xview)
        self.canvas.configure(yscrollcommand=ys.set,xscrollcommand=xs.set)
        self.canvas.grid(row=0,column=0,sticky='nsew');ys.grid(row=0,column=1,sticky='ns');xs.grid(row=1,column=0,sticky='ew')
        self.canvas.bind('<Configure>',lambda e:self.show_image())
        self.canvas.bind('<KeyPress>',lambda e:self.key(e,True));self.canvas.bind('<KeyRelease>',lambda e:self.key(e,False))
        self.canvas.bind('<Motion>',self.motion);self.canvas.bind('<FocusOut>',lambda e:self.release_controls())
        for button,mask in ((1,1),(2,4),(3,2)):
            self.canvas.bind(f'<ButtonPress-{button}>',lambda e,m=mask:self.mouse(e,m,True))
            self.canvas.bind(f'<ButtonRelease-{button}>',lambda e,m=mask:self.mouse(e,m,False))
        ttk.Label(root,textvariable=self.status,padding=8,wraplength=1000).pack(fill='x')
        self.refresh();root.protocol('WM_DELETE_WINDOW',self.close);root.after(35,self.pump);root.after(350,self.connect)

    def refresh(self):
        self.ports['values']=('Auto',)+tuple(p.device for p in sorted(list_ports.comports(),key=lambda p:p.device))

    def post(self,kind,value):
        # Frames never compete with connection state and only the latest waits.
        if kind=='frame':
            with self.frame_lock:self.pending_frame=value
            return
        if kind in ('lost','done'):
            with self.frame_lock:self.pending_frame=None
        self.events.put_nowait((kind,value))

    def connect(self):
        if self.thread and self.thread.is_alive():return
        self.stop=threading.Event();self.last_frame_at=None;self.release_controls()
        self.divisor=2 if self.quality.get()=='Half' else 1
        self.connect_button.state(['disabled']);self.status.set('Searching for Freslest...')
        args=(self.port.get().strip() or 'Auto',int(self.speed.get()),self.divisor,self.stop)
        self.thread=threading.Thread(target=self.receive,args=args,daemon=True);self.thread.start()

    def receive(self,port,baud,divisor,stop):
        try:
            while not stop.is_set():
                choices=[p.device for p in candidates()] if port=='Auto' else [port]
                if not choices:self.post('search','Waiting for a USB device. Native: Settings > Display > USB PC Viewer: ON.')
                for device in choices:
                    if stop.is_set():break
                    c=None;negotiated=False
                    try:
                        c,transport,size=open_device(device,stop)
                        c.write(f'FS1 {baud} {divisor}\n'.encode('ascii'))
                        ack=bytearray();deadline=time.monotonic()+2
                        while not stop.is_set():
                            ack.extend(c.read(512))
                            if f'FSREADY {baud}\n'.encode() in ack or f'FSREADY {baud}\r\n'.encode() in ack:break
                            if b'FSERROR' in ack or b'FSBUSY' in ack:raise RuntimeError('Screen memory unavailable or another viewer is connected.')
                            if time.monotonic()>deadline:raise RuntimeError('Screen handshake timed out. Reset the board and retry.')
                            if len(ack)>4096:del ack[:-512]
                        if stop.is_set():break
                        if transport=='UART':c.baudrate=baud
                        negotiated=True;self.connection_label=f'{transport} · {device}'
                        self.post('connected',(device,transport,size));self.stream(c,stop)
                    except (serial.SerialException,OSError,RuntimeError,ValueError) as error:
                        self.post('error',str(error) if negotiated or port!='Auto' else f'Checking USB ports... ({device})')
                    finally:
                        if c and c.is_open:
                            try:c.write(b'FSR\nFS0\n')
                            except (serial.SerialException,OSError):pass
                            try:c.close()
                            except (serial.SerialException,OSError):pass
                        self.controls.release()
                        if negotiated:self.post('lost',None)
                stop.wait(1)
        finally:self.post('done',None)

    def stream(self,c,stop):
        c.timeout=.012
        decoder=Decoder();c.write(b'FS2\n');waiting=True;next_request=0;deadline=time.monotonic()+5
        last_revision=-1;last_control=0
        while not stop.is_set():
            now=time.monotonic();state=self.controls.snapshot()
            if now-last_control>=.02 and (state[0]!=last_revision or now-last_control>=.25):
                c.write(control_commands(state));last_revision=state[0];last_control=now
            data=c.read(min(8192,max(1,getattr(c,'in_waiting',0))))
            if data:
                frames=decoder.feed(data)
                if frames:
                    seq,w,h,pixels=frames[-1]
                    self.post('frame',(w,h,ppm(w,h,pixels),seq));waiting=False
                    next_request=time.monotonic()+.035;deadline=time.monotonic()+5
            now=time.monotonic()
            if not waiting and now>=next_request:
                c.write(b'FS2\n');waiting=True;deadline=now+5
            if now>deadline:raise RuntimeError('Transfer stopped. Return to Freslest OS; Doom/updater run separate firmware. Reconnecting...')

    def disconnect(self):
        self.release_controls();self.stop.set();self.connected=False
        self.status.set('Disconnecting. UART returns to 115200 after its pending frame / idle timeout.')

    def pump(self):
        # Bound each callback: serial traffic must not starve Tk input/timers.
        for _ in range(16):
            try:kind,value=self.events.get_nowait()
            except queue.Empty:break
            if kind=='connected':
                self.connected=True;self.frame_size=tuple(n//self.divisor for n in value[2]);self.release_controls()
                self.last_frame_at=None;self._status_at=0
            elif kind=='lost':self.connected=False;self.release_controls()
            elif kind in ('error','search'):self.status.set(value)
            elif kind=='done':self.connected=False;self.release_controls();self.connect_button.state(['!disabled'])
        with self.frame_lock:
            frame=self.pending_frame;self.pending_frame=None
        if frame is not None and self.connected:
            w,h,data,seq=frame;self.frame_size=(w,h)
            if self.image is None:self.image=tk.PhotoImage(data=data,format='PPM')
            else:self.image.configure(width=w,height=h,data=data,format='PPM')
            self._revision+=1;self.show_image();now=time.monotonic()
            elapsed=now-self.last_frame_at if self.last_frame_at is not None else 0
            fps=1/elapsed if elapsed>0 else 0;self.last_frame_at=now
            if now-self._status_at>=.5:
                self._status_at=now
                self.status.set(f'Live · {self.connection_label} · {w} × {h} · {fps:.1f} fps · ESP frame/transfer buffer: PSRAM · Click screen to control')
        self.root.after(33,self.pump)

    def show_image(self,reset=False):
        cw,ch=self.canvas.winfo_width(),self.canvas.winfo_height()
        if self.image is None:
            if self._placeholder is None:
                self._placeholder=self.canvas.create_text(cw//2,ch//2,text='FRESLEST\nUSB Screen Viewer',fill='#bccbdf',font=('Segoe UI',22),justify='center')
            else:self.canvas.coords(self._placeholder,cw//2,ch//2)
            return
        w,h=self.image.width(),self.image.height()
        self.layout=geometry(w,h,cw,ch,self.zoom.get());x,y,dw,dh,n,d=self.layout
        signature=(self._revision,w,h,n,d)
        if signature!=self._render_signature:
            if n==d==1:self.display_image=self.image
            else:
                if self._scaled_image is None:self._scaled_image=tk.PhotoImage(width=dw,height=dh)
                elif (self._scaled_image.width(),self._scaled_image.height())!=(dw,dh):self._scaled_image.configure(width=dw,height=dh)
                self._scaled_image.tk.call(str(self._scaled_image),'copy',str(self.image),'-zoom',n,n,'-subsample',d,d,'-compositingrule','set')
                self.display_image=self._scaled_image
            self._render_signature=signature
        if self._placeholder is not None:self.canvas.itemconfigure(self._placeholder,state='hidden')
        if self._canvas_image is None:self._canvas_image=self.canvas.create_image(x,y,image=self.display_image,anchor='nw')
        else:self.canvas.itemconfigure(self._canvas_image,image=self.display_image)
        viewport=(x,y,dw,dh,cw,ch)
        if viewport!=self._viewport or reset:
            self.canvas.coords(self._canvas_image,x,y)
            self.canvas.configure(scrollregion=(0,0,max(x+dw,cw),max(y+dh,ch)))
            if reset or self.zoom.get()=='Fit':self.canvas.xview_moveto(0);self.canvas.yview_moveto(0)
            self._viewport=viewport

    def point(self,event):
        if self.layout is None:return None
        return source_point(self.canvas.canvasx(event.x),self.canvas.canvasy(event.y),self.layout,*self.frame_size,self.divisor)

    def key(self,event,down):
        if self.connected and self.control_enabled.get() and self.canvas.focus_get()==self.canvas:
            if self.controls.key(event.keysym,down):return 'break'

    def motion(self,event):
        if self.connected and self.control_enabled.get() and self.canvas.focus_get()==self.canvas:
            point=self.point(event)
            if point is not None:self.controls.mouse(point);self.canvas.configure(cursor='none')

    def mouse(self,event,button,down):
        if not (self.connected and self.control_enabled.get()):return
        point=self.point(event)
        if down and point is None:return
        if down:self.canvas.focus_set();self.canvas.configure(cursor='none')
        self.controls.mouse(point,button,down)
        return 'break'

    def release_controls(self):
        self.controls.release()
        if hasattr(self,'canvas'):self.canvas.configure(cursor='arrow')

    def save(self):
        if self.image is None:messagebox.showinfo('Save PNG','Connect and receive a screen first.');return
        path=filedialog.asksaveasfilename(defaultextension='.png',filetypes=[('PNG image','*.png')],initialfile='freslest-screen.png')
        if path:self.image.write(path,format='png')

    def close(self):
        self.release_controls();self.stop.set();self.root.destroy()


if __name__=='__main__':
    root=tk.Tk();Viewer(root);root.mainloop()
