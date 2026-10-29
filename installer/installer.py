"""Freslest OS 1.1 Windows installer with Turkish and English interfaces."""
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
from translations import LANGUAGES, translate

ROOT = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1]))
FW = ROOT / 'firmware'
from payload import DISPLAY_MODELS, ITEMS
from payload import validated_payload as _validated_payload
from sd_audio import install_audio, validate_card_volume, validated_audio
AUDIO = ROOT / 'audio_pack'


def validated_payload(language='tr', display='ili9488'):
    return _validated_payload(FW, language, display)

def write_diagnostics(output_path):
    """Export a support report without opening ports or creating a GUI."""
    report={'ok':False,'release':'1.1','frozen':bool(getattr(sys,'frozen',False)),
            'installer_languages':list(LANGUAGES),'errors':[]}
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
        report['sd_audio_files']=len(validated_audio(AUDIO))
        image=(FW/'freslest_os_s3.bin').read_bytes()
        report['firmware_version']=image[0x30:0x50].split(b'\0',1)[0].decode('ascii')
        if image[0x20:0x24]!=b'\x32\x54\xcd\xab' or report['firmware_version']!='1.1':
            raise RuntimeError('Bundled OS application descriptor must report 1.1')
        report['ok']=True
    except Exception as error:
        report['errors'].append(type(error).__name__+': '+str(error))
    Path(output_path).write_text(json.dumps(report,indent=2),encoding='utf-8')
    return 0 if report['ok'] else 1

class Installer(tk.Tk):
    def __init__(self,language='tr'):
        super().__init__()
        self.language_code=language if language in LANGUAGES else 'tr'
        self.title('Freslest Installer | OS 1.1')
        self.geometry('690x560'); self.minsize(660,520)
        self.configure(bg='#121724')
        self.port = tk.StringVar()
        self.baud = tk.StringVar(value='460800')
        self.display = tk.StringVar(value='ili9488')
        self.sd_target = tk.StringVar()
        self.sd_running = False
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
    def t(self,key,**values):
        return translate(key,self.__dict__.get('language_code','tr'),**values)
    def select_language(self,event=None):
        if self.busy:return
        self.language_code=next(code for code,label in LANGUAGES.items() if label==self.language_selection.get())
        self.wizard()
    def wizard(self):
        self.wipe()
        self.title('Freslest Installer | OS 1.1'+(' | English' if self.language_code=='en' else ''))
        bar=tk.Frame(self,bg='#121724');bar.pack(fill='x',padx=34,pady=(14,0))
        tk.Label(bar,text='Dil / Language',font=('Segoe UI',10),fg='#dce6fa',bg='#121724').pack(side='left')
        self.language_selection=tk.StringVar(value=LANGUAGES[self.language_code])
        picker=ttk.Combobox(bar,textvariable=self.language_selection,values=tuple(LANGUAGES.values()),state='readonly',width=12)
        picker.pack(side='left',padx=12);picker.bind('<<ComboboxSelected>>',self.select_language)
        self.label('FRESLEST',30,'#70b1ff',True).pack(anchor='w',padx=34,pady=(18,0))
        self.label('OS 1.1  |  ESP32-S3 N16R8',18,'#ffffff',True).pack(anchor='w',padx=34,pady=8)
        self.label(self.t('welcome')).pack(anchor='w',padx=34,pady=18)
        self.label(self.t('steps'),13).pack(anchor='w',padx=34,pady=8)
        self.label(self.t('backup'),11,'#ffca80').pack(anchor='w',padx=34,pady=(12,22))
        actions=tk.Frame(self,bg='#121724');actions.pack(fill='x',padx=35)
        ttk.Button(actions,text=self.t('sd_button'),command=self.sd_page).pack(side='left')
        ttk.Button(actions,text=self.t('continue'),command=self.displays).pack(side='right')
    def sd_page(self):
        self.wipe()
        self.label(self.t('sd_heading'),23,'#70b1ff',True).pack(anchor='w',padx=32,pady=(28,14))
        self.label(self.t('sd_help')).pack(anchor='w',padx=32,pady=10)
        line=tk.Frame(self,bg='#121724');line.pack(fill='x',padx=32,pady=18)
        ttk.Entry(line,textvariable=self.sd_target,state='readonly').pack(side='left',fill='x',expand=True)
        ttk.Button(line,text=self.t('sd_choose'),command=self.choose_sd).pack(side='right',padx=(10,0))
        self.label(self.t('sd_contents'),12).pack(anchor='w',padx=32,pady=8)
        self.label(self.t('sd_notice'),11,'#ffca80').pack(anchor='w',padx=32,pady=14)
        buttons=tk.Frame(self,bg='#121724');buttons.pack(side='bottom',fill='x',padx=32,pady=26)
        ttk.Button(buttons,text=self.t('back'),command=self.wizard).pack(side='left')
        ttk.Button(buttons,text=self.t('sd_button'),command=self.confirm_sd).pack(side='right')
    def choose_sd(self):
        target=filedialog.askdirectory(title=self.t('sd_choose'),mustexist=True)
        if target:self.sd_target.set(target)
    def confirm_sd(self):
        try:card=validate_card_volume(self.sd_target.get())
        except (OSError,ValueError) as error:
            messagebox.showerror(self.t('sd_heading'),str(error));return
        if not messagebox.askyesno(self.t('sd_heading'),self.t('sd_confirm',path=str(card))):return
        self.busy=self.sd_running=True
        self.wipe()
        self.label(self.t('sd_copying'),21,'#70b1ff',True).pack(anchor='w',padx=32,pady=(28,16))
        self.progress=ttk.Progressbar(self,mode='determinate',maximum=112)
        self.progress.pack(fill='x',padx=32,pady=12)
        self.log=tk.Text(self,height=17,bg='#080d18',fg='#d5e6fa',font=('Consolas',9),state='disabled')
        self.log.pack(fill='both',expand=True,padx=32,pady=10)
        self.finish=ttk.Button(self,text=self.t('close'),command=self.close,state='disabled')
        self.finish.pack(anchor='e',padx=32,pady=16)
        threading.Thread(target=self.sd_worker,args=(card,),daemon=True).start()
    def sd_worker(self,card):
        try:
            result=install_audio(AUDIO,card,lambda number,total,name:self.events.put(('sd_progress',number,total,name)))
            self.events.put(('sd_done',True,self.t('sd_complete',copied=result['copied'],verified=result['verified'])))
        except Exception as error:self.events.put(('sd_done',False,str(error)))
    def sd_result(self,success,message):
        self.last_log=self.log.get('1.0','end-1c')
        self.wipe()
        self.label(self.t('sd_success' if success else 'sd_failure'),23,'#81dfb0' if success else '#ff9f91',True).pack(anchor='w',padx=32,pady=(30,16))
        self.label(message).pack(anchor='w',padx=32,pady=12)
        self.label(self.t('sd_next' if success else 'sd_retry_help')).pack(anchor='w',padx=32,pady=12)
        buttons=tk.Frame(self,bg='#121724');buttons.pack(side='bottom',fill='x',padx=32,pady=26)
        ttk.Button(buttons,text=self.t('back'),command=self.wizard if success else self.sd_page).pack(side='left')
        ttk.Button(buttons,text=self.t('close'),command=self.close).pack(side='right')
    def displays(self):
        self.wipe()
        self.label(self.t('displays_heading'),21,'#70b1ff',True).pack(anchor='w',padx=32,pady=(20,8))
        self.label(self.t('display_help')).pack(anchor='w',padx=32,pady=8)
        for model,title in DISPLAY_MODELS.items():
            tk.Radiobutton(self,text=title,variable=self.display,value=model,
                           bg='#121724',fg='#dce6fa',activebackground='#121724',
                           activeforeground='white',selectcolor='#283247',
                           font=('Segoe UI',13),anchor='w').pack(fill='x',padx=38,pady=5)
        self.label(self.t('display_hint'),11,'#ffca80').pack(anchor='w',padx=32,pady=10)
        buttons=tk.Frame(self,bg='#121724');buttons.pack(side='bottom',fill='x',padx=32,pady=26)
        ttk.Button(buttons,text=self.t('back'),command=self.wizard).pack(side='left')
        ttk.Button(buttons,text=self.t('continue'),command=self.ports).pack(side='right')
    def ports(self):
        self.wipe();self.label(self.t('ports_heading'),21,'#70b1ff',True).pack(anchor='w',padx=32,pady=(24,9))
        self.label(self.t('connect_help')).pack(anchor='w',padx=32,pady=8)
        line=tk.Frame(self,bg='#121724');line.pack(fill='x',padx=32,pady=15)
        self.combos=ttk.Combobox(line,textvariable=self.port,state='normal',width=35)
        self.combos.pack(side='left',fill='x',expand=True)
        ttk.Button(line,text=self.t('refresh'),command=self.refresh_ports).pack(side='left',padx=(10,0))
        self.refresh_ports(auto_select=not bool(self.port.get()))
        speed=tk.Frame(self,bg='#121724');speed.pack(fill='x',padx=32,pady=(0,12))
        tk.Label(speed,text=self.t('baud'),fg='#dce6fa',bg='#121724',font=('Segoe UI',11)).pack(side='left')
        ttk.Combobox(speed,textvariable=self.baud,values=('460800','115200','921600'),state='readonly',width=10).pack(side='left',padx=10)
        self.label(self.t('port_hint')).pack(anchor='w',padx=32)
        self.label(self.t('overwrite'),11,'#ffca80').pack(anchor='w',padx=32,pady=(20,4))
        buttons=tk.Frame(self,bg='#121724');buttons.pack(side='bottom',fill='x',padx=32,pady=26)
        ttk.Button(buttons,text=self.t('back'),command=self.displays).pack(side='left')
        ttk.Button(buttons,text=self.t('continue'),command=self.confirm).pack(side='right')
    def refresh_ports(self,auto_select=True):
        ports=list(list_ports.comports())
        self.combos['values']=[p.device for p in ports]
        if auto_select and ports and self.port.get() not in self.combos['values']:self.port.set(ports[0].device)
    def confirm(self):
        port=self.port.get().strip().upper()
        if not port.startswith('COM') or not port[3:].isdigit():
            messagebox.showerror(self.t('port_title'),self.t('invalid_port'));return
        if not messagebox.askyesno(self.t('confirm_title'),self.t('confirm',port=port,display=DISPLAY_MODELS[self.display.get()])):return
        self.flash(port)
    def flash(self,port):
        self.busy=True
        self.sd_running=False
        self.wipe();self.label(self.t('flash_heading'),19,'#70b1ff',True).pack(anchor='w',padx=32,pady=(24,12))
        self.progress=ttk.Progressbar(self,mode='indeterminate');self.progress.pack(fill='x',padx=32,pady=9);self.progress.start(10)
        self.log=tk.Text(self,height=17,bg='#080d18',fg='#d5e6fa',insertbackground='white',font=('Consolas',9),state='disabled')
        self.log.pack(fill='both',expand=True,padx=32,pady=10)
        self.finish=ttk.Button(self,text=self.t('close'),command=self.close,state='disabled');self.finish.pack(side='right',padx=32,pady=(0,16))
        # Read Tk state on the GUI thread; freeze the selection for this flash.
        threading.Thread(target=self.worker,args=(port,int(self.baud.get()),self.display.get()),daemon=True).start()
    def worker(self,port,baud=460800,display='ili9488'):
        try:
            args=['--chip','esp32s3','--port',port,'--baud',str(baud),'--before','default_reset','--after','hard_reset',
                  'write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','16MB']
            for addr,path,_,_ in validated_payload(self.__dict__.get('language_code','tr'),display):
                args.extend([hex(addr),str(path)])
                self.events.put(self.t('verified',name=path.name))
            self.events.put(self.t('connecting',port=port))
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
            self.events.put(('done',True,self.t('completed_detail')))
        except BaseException as ex:
            self.events.put(('done',False,str(ex)))
    def poll(self):
        try:
            for _ in range(100):
                v=self.events.get_nowait()
                if isinstance(v,tuple) and v[0]=='sd_progress':
                    self.progress.configure(value=v[1],maximum=v[2]);self.write_log(v[3]+'\n')
                elif isinstance(v,tuple) and v[0]=='sd_done':
                    self.busy=self.sd_running=False
                    self.progress.configure(value=112 if v[1] else 0)
                    self.sd_result(v[1],v[2]);break
                elif isinstance(v,tuple) and v[0]=='done':
                    self.busy=False
                    self.progress.stop();self.progress.configure(mode='determinate',value=100 if v[1] else 0)
                    self.finish.configure(state='normal')
                    self.write_log(self.t('success_prefix' if v[1] else 'error_prefix')+v[2]+'\n')
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
        self.label(self.t('success_heading' if success else 'failure_heading'),24,'#81dfb0' if success else '#ff9f91',True).pack(anchor='w',padx=32,pady=(36,16))
        self.label(self.t('success_summary' if success else 'failure_summary'),16,bold=True).pack(anchor='w',padx=32,pady=8)
        self.label(message,12).pack(anchor='w',padx=32,pady=10)
        self.label(self.t('success_next' if success else 'failure_next'),12).pack(anchor='w',padx=32,pady=12)
        ttk.Button(self,text=self.t('close'),command=self.close).pack(side='bottom',anchor='e',padx=32,pady=24)
        ttk.Button(self,text=self.t('save_log'),command=self.save_log).pack(side='bottom',anchor='w',padx=32,pady=4)
        if not success:ttk.Button(self,text=self.t('retry'),command=self.ports).pack(side='bottom',anchor='w',padx=32,pady=4)
    def save_log(self):
        path=filedialog.asksaveasfilename(title=self.t('save_title'),defaultextension='.txt',initialfile='freslest-install-log.txt',filetypes=[(self.t('text_file'),'*.txt')])
        if path:
            try:Path(path).write_text(self.last_log,encoding='utf-8')
            except OSError as error:messagebox.showerror(self.t('save_error'),str(error))
    def close(self):
        if self.busy:
            messagebox.showinfo(self.t('busy_title'),self.t('sd_busy' if self.sd_running else 'busy'))
            return
        self.destroy()
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description='Freslest OS 1.1 Installer')
    parser.add_argument('--lang',choices=tuple(LANGUAGES),default='tr')
    parser.add_argument('--diagnostics',metavar='REPORT_JSON')
    options=parser.parse_args()
    if options.diagnostics:raise SystemExit(write_diagnostics(options.diagnostics))
    Installer(options.lang).mainloop()
