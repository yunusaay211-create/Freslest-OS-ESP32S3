"""Create eight original Freslest UI cues. Requires numpy and ffmpeg.

Only MP3/0105.mp3 through MP3/0112.mp3 are replaced. Doom is untouched.
"""
from pathlib import Path
import hashlib,json,subprocess,tempfile,wave
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
RATE=44100

def note(audio,start,duration,freq,gain=1.,pan=0.,glide=1.):
    n=int(duration*RATE);t=np.arange(n)/RATE
    phase=2*np.pi*freq*(t+(glide-1)*t*t/(2*duration))
    body=np.sin(phase+0.12*np.sin(2*phase))*np.exp(-t/max(duration*.34,.03))
    body+=.15*np.sin(2*phase)*np.exp(-t/max(duration*.13,.015))
    env=(1-np.exp(-t/.004))*np.minimum(1.,np.maximum(0.,(duration-t)/.045))
    body*=env*gain
    offset=int(start*RATE);n=min(n,len(audio)-offset)
    audio[offset:offset+n,0]+=body[:n]*np.sqrt((1-pan)/2)
    audio[offset:offset+n,1]+=body[:n]*np.sqrt((1+pan)/2)

C5,E5,G5,A5,C6=523.251,659.255,783.991,880.,1046.502
CUES={
 105:('Startup',1.18,[(0,.65,C5,.62,-.1,1),(.11,.70,E5,.55,.1,1),(.25,.86,G5,.54,0,1)],.40),
 106:('Soft click',.13,[(0,.105,880.,.65,0,.88),(0,.08,440.,.26,0,1)],.21),
 107:('Jump',.23,[(0,.19,600.,.60,0,1.42)],.32),
 108:('Pickup',.30,[(0,.17,G5,.60,-.08,1),(.07,.20,C6,.44,.08,1)],.33),
 109:('Tile lock',.31,[(0,.23,440.,.6,0,1),(0,.18,880.,.25,0,1),(.07,.18,E5,.28,0,1)],.31),
 110:('Game start',.64,[(0,.32,C5,.56,-.08,1),(.09,.35,E5,.5,0,1),(.19,.40,C6,.42,.08,1)],.38),
 111:('Game over',.86,[(0,.40,E5,.48,-.06,1),(.16,.44,C5,.5,.06,1),(.33,.46,391.995,.42,0,1)],.36),
 112:('Win',.94,[(0,.44,C5,.52,-.1,1),(.10,.48,E5,.46,0,1),(.20,.50,G5,.44,.1,1),(.34,.53,C6,.38,0,1)],.39),
}

def main():
    report={'version':'6.00','source':'Original procedural Freslest cues; no Doom samples','sample_rate':RATE,'files':{}}
    with tempfile.TemporaryDirectory(prefix='freslest-sounds-') as temporary:
        for number,(title,duration,notes,peak) in CUES.items():
            audio=np.zeros((int(duration*RATE),2),np.float64)
            for args in notes:note(audio,*args)
            dry=audio.copy()
            for seconds,amount in ((.019,.10),(.033,.045)):
                shift=int(seconds*RATE);audio[shift:]+=dry[:-shift,::-1]*amount
            audio-=audio.mean(axis=0);audio*=peak/np.max(np.abs(audio))
            fade=min(int(.012*RATE),len(audio)//2)
            audio[:fade]*=np.linspace(0,1,fade)[:,None];audio[-fade:]*=np.linspace(1,0,fade)[:,None]
            wav=Path(temporary)/f'{number}.wav'
            with wave.open(str(wav),'wb') as stream:
                stream.setnchannels(2);stream.setsampwidth(2);stream.setframerate(RATE)
                stream.writeframes(np.rint(audio*32767).astype('<i2').tobytes())
            output=ROOT/'MP3'/f'{number:04}.mp3'
            subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-i',str(wav),'-c:a','libmp3lame','-b:a','192k','-ar',str(RATE),'-ac','2','-id3v2_version','3','-metadata','artist=Freslest','-metadata','album=Freslest OS 6.00','-metadata',f'title={title}',str(output)],check=True)
            decoded=subprocess.check_output(['ffmpeg','-hide_banner','-loglevel','error','-i',str(output),'-f','f32le','-ac','2','-ar',str(RATE),'-'])
            samples=np.frombuffer(decoded,dtype='<f4');actual_peak=float(np.max(np.abs(samples)));rms=float(np.sqrt(np.mean(samples*samples)))
            assert .05<actual_peak<.65 and rms>.01
            report['files'][output.name]={'event':title,'duration_seconds':len(samples)/(2*RATE),'peak_dbfs':round(20*np.log10(actual_peak),2),'rms_dbfs':round(20*np.log10(rms),2),'sha256':hashlib.sha256(output.read_bytes()).hexdigest()}
            print(f'{output.name}: {title}; peak {20*np.log10(actual_peak):.1f} dBFS')
    (ROOT/'OS_SOUNDS.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
