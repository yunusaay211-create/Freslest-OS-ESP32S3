"""Viewport geometry and thread-safe PC input for Freslest Screen 2."""
import threading

KEYS = {chr(97+i):4+i for i in range(26)}
KEYS.update({c:30+i for i,c in enumerate('1234567890')})
KEYS.update({'Return':0x28,'KP_Enter':0x28,'Escape':0x29,'BackSpace':0x2a,'Tab':0x2b,'space':0x2c,
 'minus':0x2d,'underscore':0x2d,'equal':0x2e,'plus':0x2e,'bracketleft':0x2f,'braceleft':0x2f,
 'bracketright':0x30,'braceright':0x30,'backslash':0x31,'bar':0x31,'semicolon':0x33,'colon':0x33,
 'apostrophe':0x34,'quotedbl':0x34,'grave':0x35,'asciitilde':0x35,'comma':0x36,'less':0x36,
 'period':0x37,'greater':0x37,'slash':0x38,'question':0x38,'Caps_Lock':0x39,
 'Insert':0x49,'Home':0x4a,'Prior':0x4b,'Delete':0x4c,'End':0x4d,'Next':0x4e,
 'Right':0x4f,'Left':0x50,'Down':0x51,'Up':0x52,
 'Control_L':0xe0,'Shift_L':0xe1,'Alt_L':0xe2,'Super_L':0xe3,
 'Control_R':0xe4,'Shift_R':0xe5,'Alt_R':0xe6,'ISO_Level3_Shift':0xe6,'Super_R':0xe7})
KEYS.update({f'F{i+1}':0x3a+i for i in range(12)})
KEYS.update(dict(zip(('exclam','at','numbersign','dollar','percent','asciicircum','ampersand','asterisk','parenleft','parenright'),range(30,40))))


def usage(keysym):
    return KEYS.get(keysym, KEYS.get(keysym.lower()))


def geometry(width, height, canvas_width, canvas_height, zoom='Fit'):
    cw, ch = max(1,canvas_width), max(1,canvas_height)
    if zoom == 'Fit':
        num, den = 1, max(1,(width+cw-1)//cw,(height+ch-1)//ch)
        for n,d in ((1,4),(1,3),(1,2),(1,1),(2,1),(3,1)):
            if (width*n+d-1)//d <= cw and (height*n+d-1)//d <= ch:
                num,den=n,d
    else:
        num,den=int(zoom[0]),1
    dw, dh = (width*num+den-1)//den, (height*num+den-1)//den
    return max(0,(cw-dw)//2),max(0,(ch-dh)//2),dw,dh,num,den


def source_point(x,y,layout,width,height,divisor):
    ox,oy,dw,dh,n,d=layout
    if not (ox<=x<ox+dw and oy<=y<oy+dh):
        return None
    return (min(width*divisor-1,int((x-ox)*d/n)*divisor),
            min(height*divisor-1,int((y-oy)*d/n)*divisor))


class Controls:
    def __init__(self):
        self.lock=threading.Lock()
        self.held=set()
        self.pointer=None
        self.buttons=0
        self.revision=0

    def key(self,keysym,down):
        u=usage(keysym)
        if u is None:
            return False
        with self.lock:
            old=u in self.held
            if down:self.held.add(u)
            else:self.held.discard(u)
            if old!=down:self.revision+=1
        return True

    def mouse(self,point,button=0,down=False):
        with self.lock:
            old=(self.pointer,self.buttons)
            if point is not None:self.pointer=point
            if button:
                if down:self.buttons|=button
                else:self.buttons&=~button
            if old!=(self.pointer,self.buttons):self.revision+=1

    def release(self):
        with self.lock:
            self.held.clear();self.pointer=None;self.buttons=0;self.revision+=1

    def snapshot(self):
        with self.lock:
            keys=bytearray(32)
            for u in self.held:keys[u//8]|=1<<(u%8)
            return self.revision,bytes(keys),self.pointer,self.buttons


def control_commands(snapshot):
    _,keys,point,buttons=snapshot
    if not any(keys) and point is None:
        return b'FSR\n'
    out=b'FSK '+keys.hex().encode('ascii')+b'\n'
    if point is not None:
        out+=f'FSP {point[0]} {point[1]} {buttons}\n'.encode('ascii')
    return out
