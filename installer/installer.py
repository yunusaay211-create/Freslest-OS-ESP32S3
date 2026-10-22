"""Freslest OS 1.0 Windows installer; validated original firmware payload."""
import os
import sys
# pythonw/PyInstaller windowed applications have no standard streams.
for _stream in ('stdout', 'stderr'):
    if getattr(sys, _stream) is None:
        setattr(sys, _stream, open(os.devnull, 'w', encoding='utf-8'))
import contextlib
import hashlib, json, queue, threading, tkinter as tk
from pathlib import Path
from tkinter import ttk, messagebox, filedialog
from serial.tools import list_ports

ROOT = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1]))
FW = ROOT / 'firmware'
ITEMS = [
    (0x000000, 'bootloader.bin',0x8000),
    (0x008000, 'partition-table.bin',0x1000),
    (0x010000, 'ota_data_initial.bin',0x2000),
    (0x020000, 'freslest_doom_s3.bin',0xE0000),
    (0x100000, 'freslest_os_s3.bin',0x1B0000),
    (0x460000, 'doom_flash.wad',0x950000),
    (0xDB0000, 'littlefs.bin',0x50000),
]

def validated_payload():
    manifest=json.loads((FW/'SHA256.json').read_text(encoding='utf-8'))
    result=[]
    for address,name,limit in ITEMS:
        path=FW/name;data=path.read_bytes();digest=hashlib.sha256(data).hexdigest()
        if not data or len(data)>limit or digest!=manifest.get('firmware/'+name):
            raise RuntimeError('Dosya hatalı veya eksik: '+name)
        result.append((address,path,len(data),digest))
    return result

def write_diagnostics(output_path):
    """Export a support report without opening ports or creating a GUI."""
    report={'ok':False,'release':'1.0','frozen':bool(getattr(sys,'frozen',False)),'errors':[]}
    try:
        import esptool
        import serial
        from esptool.loader import StubFlasher
        from esptool.targets.esp32s3 import ESP32S3ROM
        # Pass the class: constructing a ROM loader would open a serial port.
        stub=StubFlasher(ESP32S3ROM)
        report.update(esptool_version=esptool.__version__,pyserial_version=serial.__version__,
                      chip=ESP32S3ROM.CHIP_NAME,stub_text_bytes=len(stub.text),
                      stub_data_bytes=len(stub.data),
                      stub_entry_in_text=stub.text_start<=stub.entry<stub.text_start+len(stub.text))
        if esptool.__version__!='4.11.0' or serial.__version__!='3.5':
            raise RuntimeError('Unexpected flash dependency version')
        if not stub.text or not stub.data or not report['stub_entry_in_text']:
            raise RuntimeError('ESP32-S3 stub resource is invalid')
        report['firmware']={path.name:{'address':hex(address),'bytes':size,'sha256':digest,'valid':True}
                            for address,path,size,digest in validated_payload()}
        report['ok']=True
    except Exception as error:
        report['errors'].append(type(error).__name__+': '+str(error))
    Path(output_path).write_text(json.dumps(report,indent=2),encoding='utf-8')
    return 0 if report['ok'] else 1

class Installer(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('Freslest Installer | OS 1.0')
        self.geometry('660x510'); self.minsize(610,475)
        self.configure(bg='#121724')
        self.port = tk.StringVar()
        self.baud = tk.StringVar(value='460800')
        self.busy = False
        self.events=queue.Queue()
        self.make_style()
        self.wizard()
        self.protocol('WM_DELETE_WINDOW', self.close)
        self.after(80,self.poll)
    def make_style(self):
        style=ttk.Style(self);style.theme_use('clam')
        style.configure('TButton',font=('Segoe UI',11),padding=9)
        style.configure('TCombobox',font=('Segoe UI',11),padding=5)
        style.configure('Horizontal.TProgressbar',troughcolor='#283247',background='#4f9cff')
    def wipe(self):
        for w in self.winfo_children():w.destroy()
    def label(self, text, size=12, color='#dce6fa', bold=False):
        return tk.Label(self,text=text,font=('Segoe UI',size,'bold' if bold else 'normal'),fg=color,bg='#121724',justify='left',wraplength=590)
    def wizard(self):
        self.wipe();self.label('FRESLEST',30,'#70b1ff',True).pack(anchor='w',padx=34,pady=(36,0))
        self.label('OS 1.0  |  ESP32-S3 N16R8',18,'#ffffff',True).pack(anchor='w',padx=34,pady=8)
        self.label('Tek tıklamalı Windows yükleyicisine hoş geldin. ESP-IDF veya Python kurman gerekmiyor.').pack(anchor='w',padx=34,pady=18)
        self.label('• Adım 1: Başlangıç\n• Adım 2: COM portunu seç\n• Adım 3: Firmware yükle',13).pack(anchor='w',padx=34,pady=8)
        self.label('Not: Kurulum başlangıç LittleFS içeriğini değiştirir. Mevcut dosyalarını yedekle.',11,'#ffca80').pack(anchor='w',padx=34,pady=(12,22))
        ttk.Button(self,text='Devam et  →',command=self.ports).pack(anchor='e',padx=35)
    def ports(self):
        self.wipe();self.label('2 / 3   COM PORTUNU SEÇ',21,'#70b1ff',True).pack(anchor='w',padx=32,pady=(24,9))
        self.label('ESP32-S3 cihazını USB ile bağla. Gerekirse BOOT tuşunu basılı tutarak RESET yap.').pack(anchor='w',padx=32,pady=8)
        line=tk.Frame(self,bg='#121724');line.pack(fill='x',padx=32,pady=15)
        self.combos=ttk.Combobox(line,textvariable=self.port,state='normal',width=35)
        self.combos.pack(side='left',fill='x',expand=True)
        ttk.Button(line,text='Yenile',command=self.refresh_ports).pack(side='left',padx=(10,0))
        self.refresh_ports()
        speed=tk.Frame(self,bg='#121724');speed.pack(fill='x',padx=32,pady=(0,12))
        tk.Label(speed,text='Yükleme hızı:',fg='#dce6fa',bg='#121724',font=('Segoe UI',11)).pack(side='left')
        ttk.Combobox(speed,textvariable=self.baud,values=('460800','115200','921600'),state='readonly',width=10).pack(side='left',padx=10)
        self.label('USB native CDC veya USB-UART portu Windows üzerinde COM olarak görünebilir.').pack(anchor='w',padx=32)
        self.label('Yükleme tüm flash belleği topluca silmez. Ancak OS, Doom, WAD ve LittleFS bölümlerinin üzerine yazar.',11,'#ffca80').pack(anchor='w',padx=32,pady=(20,4))
        buttons=tk.Frame(self,bg='#121724');buttons.pack(side='bottom',fill='x',padx=32,pady=26)
        ttk.Button(buttons,text='← Geri',command=self.wizard).pack(side='left')
        ttk.Button(buttons,text='Devam et  →',command=self.confirm).pack(side='right')
    def refresh_ports(self):
        ports=list(list_ports.comports())
        self.combos['values']=[p.device for p in ports]
        if ports and self.port.get() not in self.combos['values']:self.port.set(ports[0].device)
    def confirm(self):
        port=self.port.get().strip().upper()
        if not port.startswith('COM') or not port[3:].isdigit():
            messagebox.showerror('COM portu','Geçerli bir Windows COM portu seç (örnek: COM7).');return
        if not messagebox.askyesno('Kurulum onayı','%s portuna Freslest 1.0 yüklensin mi?\n\nOS/Doom/WAD/LittleFS üzerine yazılacaktır. Mevcut dosyalarını yedeklediğine emin ol.'%port):return
        self.flash(port)
    def flash(self,port):
        self.busy=True
        self.wipe();self.label('3 / 3   FIRMWARE YÜKLENİYOR',19,'#70b1ff',True).pack(anchor='w',padx=32,pady=(24,12))
        self.progress=ttk.Progressbar(self,mode='indeterminate');self.progress.pack(fill='x',padx=32,pady=9);self.progress.start(10)
        self.log=tk.Text(self,height=17,bg='#080d18',fg='#d5e6fa',insertbackground='white',font=('Consolas',9),state='disabled')
        self.log.pack(fill='both',expand=True,padx=32,pady=10)
        self.finish=ttk.Button(self,text='Kapat',command=self.close,state='disabled');self.finish.pack(side='right',padx=32,pady=(0,16))
        threading.Thread(target=self.worker,args=(port,int(self.baud.get())),daemon=True).start()
    def worker(self,port,baud=460800):
        try:
            args=['--chip','esp32s3','--port',port,'--baud',str(baud),'--before','default_reset','--after','hard_reset',
                  'write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','16MB']
            for addr,path,_,_ in validated_payload():
                args.extend([hex(addr),str(path)])
                self.events.put('Doğrulandı: '+path.name+'\n')
            self.events.put('Bağlanıyor: '+port+'\n')
            class Output:
                encoding='utf-8'
                def write(_,s):
                    if s:self.events.put(s.replace('\r','\n'))
                    return len(s)
                def flush(_):pass
                def isatty(_):return False
            with contextlib.redirect_stdout(Output()),contextlib.redirect_stderr(Output()):
                import esptool
                esptool.main(args)
            self.events.put(('done',True,'Yükleme tamamlandı. Kart yeniden başlatıldı.'))
        except BaseException as ex:
            self.events.put(('done',False,str(ex)))
    def poll(self):
        try:
            for _ in range(100):
                v=self.events.get_nowait()
                if isinstance(v,tuple) and v[0]=='done':
                    self.busy=False
                    self.progress.stop();self.progress.configure(mode='determinate',value=100 if v[1] else 0)
                    self.finish.configure(state='normal')
                    self.write_log(('BAŞARILI: ' if v[1] else 'HATA: ')+v[2]+'\n')
                    self.result_page(v[1],v[2])
                    break
                else:self.write_log(v)
        except queue.Empty:pass
        self.after(80,self.poll)
    def write_log(self,s):
        self.log.configure(state='normal');self.log.insert('end',s);self.log.see('end');self.log.configure(state='disabled')
    def result_page(self,success,message):
        self.last_log=self.log.get('1.0','end-1c')
        self.wipe()
        self.label('KURULUM TAMAMLANDI' if success else 'KURULUM BAŞARISIZ',24,'#81dfb0' if success else '#ff9f91',True).pack(anchor='w',padx=32,pady=(36,16))
        self.label('Freslest OS 1.0 cihazına yüklendi.' if success else 'Cihaza bağlantı veya yükleme sırasında bir hata oluştu.',16,bold=True).pack(anchor='w',padx=32,pady=8)
        self.label(message,12).pack(anchor='w',padx=32,pady=10)
        self.label('Kart açıldıktan sonra Freslest USB Viewer.exe ile ekranını görüntüleyebilir ve kontrol edebilirsin.' if success else 'Seri monitörü ve Viewer uygulamasını kapat. Gerekirse BOOT basılıyken RESET yap; portu yeniden seç ve 115200 hızıyla dene.',12).pack(anchor='w',padx=32,pady=12)
        ttk.Button(self,text='Kapat',command=self.close).pack(side='bottom',anchor='e',padx=32,pady=24)
        ttk.Button(self,text='Günlüğü kaydet',command=self.save_log).pack(side='bottom',anchor='w',padx=32,pady=4)
        if not success:ttk.Button(self,text='Tekrar dene',command=self.ports).pack(side='bottom',anchor='w',padx=32,pady=4)
    def save_log(self):
        path=filedialog.asksaveasfilename(defaultextension='.txt',initialfile='freslest-install-log.txt',filetypes=[('Metin dosyası','*.txt')])
        if path:
            try:Path(path).write_text(self.last_log,encoding='utf-8')
            except OSError as error:messagebox.showerror('Günlük kaydedilemedi',str(error))
    def close(self):
        if self.busy:
            messagebox.showinfo('Kurulum sürüyor','Firmware yazılırken uygulamayı kapatma ve USB kablosunu çıkarma. Kurulumun bitmesini bekle.')
            return
        self.destroy()
if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--diagnostics':
        raise SystemExit(write_diagnostics(sys.argv[2]))
    Installer().mainloop()
