#include "audio_manager.h"
#include "settings.h"
#include "nes_audio.h"
#include <HardwareSerial.h>
#include <cstring>

bool soundEnabled=true;
static HardwareSerial player(1);
static constexpr int RX_PIN=17,TX_PIN=18;
static constexpr uint32_t COMMAND_GAP=120;
static constexpr uint8_t SD_FILE_COUNT=0x48; // DFRobot: 0x47 is USB
struct Command {uint8_t cmd;uint16_t value;};
static Command queue[16];
static unsigned rd=0,wr=0;
static bool initialized=false,ready=false,bootPending=false,testPending=false,stopRequested=false;
static uint32_t nextTx=0,phaseDeadline=0,priorityUntil=0,lastClick=0,verboseUntil=0,feedbackAt=0;
static unsigned phase=0,logBudget=40;
static bool feedbackReported=false,waitingForCard=false;
static bool volumePending=false;
static uint32_t healthAt=0,recoveryAfter=0;
static AudioDiagnostics diagnostics={};
static uint8_t rx[10];
static unsigned rxUsed=0;
static bool due(uint32_t now,uint32_t target) {return (int32_t)(now-target)>=0;}
static uint16_t volume() {return soundEnabled?(uint16_t)constrain((soundVolume*30+127)/255,0,30):0;}
static void enqueue(uint8_t cmd,uint16_t value) {
  unsigned next=(wr+1)%16;
  if(next==rd)return;
  queue[wr]={cmd,value};wr=next;
}
static bool verbose() {return logBudget&&!due(millis(),verboseUntil);}
static void restore(bool wake) {
  rd=wr=0;priorityUntil=0;bootPending=testPending=false;
  volumePending=stopRequested=false;
  ready=diagnostics.ready=false;waitingForCard=false;
  phase=wake?7:1;phaseDeadline=millis()+(wake?0:700);
  recoveryAfter=millis()+10000;
  ++diagnostics.recoveries;
  Serial.println("DFPLAYER: restoring SD source, DAC and current user volume");
}
static void send(uint8_t cmd,uint16_t value) {
  bool query=cmd>=0x40;
  uint8_t p[10]={0x7e,0xff,6,cmd,(uint8_t)(query?0:1),(uint8_t)(value>>8),(uint8_t)value,0,0,0xef};
  uint16_t sum=0;for(int i=1;i<=6;i++)sum+=p[i];sum=(uint16_t)(0-sum);
  p[7]=sum>>8;p[8]=sum;
  size_t written=player.write(p,sizeof(p));
  ++diagnostics.txPackets;if(written!=sizeof(p))++diagnostics.txFailures;
  if(cmd==0x12)diagnostics.lastTrack=value;
  nextTx=millis()+COMMAND_GAP;
  if(verbose()) {--logBudget;Serial.printf("DFPLAYER TX: cmd=0x%02X value=%u bytes=%u/10\n",cmd,value,(unsigned)written);}
}
static void receive(uint8_t cmd,uint16_t value) {
  // Silent RX must never trigger resets. Recover on actual module replies;
  // a successful SD query also covers a lost one-shot insertion event.
  if((cmd==0x3f||cmd==0x3a)&&(value&2)&&(ready||waitingForCard)) {
    restore(false);
  }
  if(cmd==0x3b&&(value&2)) {
    rd=wr=0;priorityUntil=0;ready=diagnostics.ready=false;waitingForCard=true;
    healthAt=millis()+2000;
  }
  diagnostics.replySeen=true;++diagnostics.rxPackets;
  if(cmd==0x3f||cmd==0x3a) {diagnostics.cardKnown=true;diagnostics.cardPresent=(value&2)!=0;}
  if(cmd==0x3b&&(value&2)) {diagnostics.cardKnown=true;diagnostics.cardPresent=false;}
  if(cmd==SD_FILE_COUNT) {
    diagnostics.cardKnown=true;diagnostics.cardPresent=value>0;diagnostics.fileCount=value;
    if(value&&waitingForCard)restore(false);
  }
  if(cmd==0x43&&ready&&value!=volume())volumePending=true;
  if(cmd==0x40) {
    diagnostics.lastError=value;
    if(value==2&&ready&&due(millis(),recoveryAfter))restore(true);
  }
  if(verbose()||(cmd==0x40&&logBudget)) {
    if(logBudget)--logBudget;
    Serial.printf("DFPLAYER RX: cmd=0x%02X value=%u%s\n",cmd,value,cmd==0x40?" (module error)":"");
  }
}
static void pollRx() {
  for(unsigned budget=0;budget<128&&player.available();++budget) {
    int value=player.read();if(value<0)break;
    if(!rxUsed&&value!=0x7e)continue;
    rx[rxUsed++]=(uint8_t)value;if(rxUsed<sizeof(rx))continue;
    uint16_t sum=0;for(int j=1;j<=6;j++)sum+=rx[j];
    uint16_t received=(uint16_t)((rx[7]<<8)|rx[8]);
    if(rx[1]==0xff&&rx[2]==6&&rx[9]==0xef&&received==(uint16_t)(0-sum)) {
      receive(rx[3],(uint16_t)((rx[5]<<8)|rx[6]));rxUsed=0;
    } else {
      ++diagnostics.invalidPackets;
      unsigned start=1;while(start<sizeof(rx)&&rx[start]!=0x7e)++start;
      rxUsed=sizeof(rx)-start;memmove(rx,rx+start,rxUsed);
    }
  }
}
void audioInit() {
  if(initialized)return;
  player.begin(9600,SERIAL_8N1,RX_PIN,TX_PIN);
  uint32_t actualBaud=player.baudRate();
  if(actualBaud<9400||actualBaud>9800) {Serial.println("DFPLAYER: UART1 initialization failed");return;}
  initialized=true;diagnostics={};
  verboseUntil=millis()+20000;logBudget=40;
  Serial.println("DFPLAYER: UART1 9600 8N1; ESP TX=GPIO18 -> module RX; module TX -> ESP RX=GPIO17");
  send(0x16,0);
  phase=0;phaseDeadline=millis()+1000;
  nesAudioSetVolume(soundEnabled,(unsigned)constrain(soundVolume,0,255));
}
static void play(uint16_t track,uint32_t duration,bool important=false) {
  // OS cues occupy their own numbered /MP3 files; never select Doom tracks.
  if(track<105||track>112)return;
  if(!soundEnabled||!initialized||!ready)return;
  uint32_t now=millis();if(!important&&!due(now,priorityUntil))return;
  if(important) {rd=wr=0;enqueue(0x06,volume());}
  enqueue(0x12,track);
  priorityUntil=now+((wr+16-rd)%16)*COMMAND_GAP+duration;
}
static void startup(uint32_t now) {
  if(!due(now,phaseDeadline)||!due(now,nextTx))return;
  switch(phase++) {
    case 0:send(0x0c,0);phaseDeadline=now+2500;break;
    case 1:send(0x09,2);phaseDeadline=now+700;break;
    case 2:send(0x1a,0);phaseDeadline=now+COMMAND_GAP;break;
    case 3:send(0x06,volume());phaseDeadline=now+COMMAND_GAP;break;
    case 4:send(SD_FILE_COUNT,0);phaseDeadline=now+COMMAND_GAP;break;
    case 5:send(0x42,0);phaseDeadline=now+COMMAND_GAP;break;
    case 7:send(0x0b,0);phase=1;phaseDeadline=now+700;break;
    default:
      ready=diagnostics.ready=true;feedbackAt=now+2000;healthAt=now+5000;
      Serial.printf("DFPLAYER: initialization complete; sound=%s volume=%u; /MP3/0105.mp3 boot cue\n",soundEnabled?"ON":"OFF",volume());
      if(testPending||bootPending) {testPending=bootPending=false;play(105,1250,true);}
      break;
  }
}
void audioUpdate(uint32_t now) {
  if(!initialized)return;
  pollRx();
  if(stopRequested&&due(now,nextTx)) {send(0x16,0);stopRequested=false;return;}
  if(!ready) {
    if(!waitingForCard)startup(now);
    else if(diagnostics.replySeen&&due(now,healthAt)&&due(now,nextTx)) {
      send(SD_FILE_COUNT,0);healthAt=now+2000;
    }
    return;
  }
  if(!feedbackReported&&due(now,feedbackAt)) {
    feedbackReported=true;
    if(!diagnostics.replySeen)Serial.println("DFPLAYER: no reply on RX17. Playback is still attempted; check module TX, shared GND and SD if silent.");
  }
  if(!due(now,nextTx))return;
  if(volumePending) {send(0x06,volume());volumePending=false;return;}
  if(rd!=wr) {
    Command c=queue[rd];rd=(rd+1)%16;
    send(c.cmd,c.cmd==0x06?volume():c.value);return;
  }
  if(diagnostics.replySeen&&due(now,healthAt)&&due(now,priorityUntil)) {
    send(SD_FILE_COUNT,0);enqueue(0x43,0);healthAt=now+5000;
  }
}
void audioStop() {
  rd=wr=0;priorityUntil=0;bootPending=testPending=false;
  if(initialized)stopRequested=true;
}
void soundApply() {
  nesAudioSetVolume(soundEnabled,(unsigned)constrain(soundVolume,0,255));
  if(!initialized)return;
  if(!soundEnabled)audioStop();
  // Retain changes even between startup's volume command and ready state.
  // Coalesce slider updates rather than filling the command ring.
  volumePending=true;
}
void audioBootMelody() {if(initialized&&!ready)bootPending=true;else play(105,1250,true);}
void audioBoot() {audioBootMelody();}
void audioClick() {uint32_t n=millis();if(n-lastClick<160)return;lastClick=n;play(106,140);}
void audioDino() {play(107,250);}
void audioSnake() {play(108,330);}
void audioTetris() {play(109,350);}
void audioGame() {play(110,700,true);}
void audioGameOver() {play(111,950,true);}
void audioWin() {play(112,1000,true);}
AudioDiagnostics audioGetDiagnostics() {return diagnostics;}
void audioPrintDiagnostics() {
  verboseUntil=millis()+30000;logBudget=40;
  Serial.printf("DFPLAYER DIAG: ready=%d sound=%d volume=%u TX=%lu RX=%lu failures=%lu invalid=%lu SD=%s files=%u error=%u recovery=%lu last=/MP3/%04u.mp3\n",
    ready,soundEnabled,volume(),(unsigned long)diagnostics.txPackets,(unsigned long)diagnostics.rxPackets,
    (unsigned long)diagnostics.txFailures,(unsigned long)diagnostics.invalidPackets,
    diagnostics.cardKnown?(diagnostics.cardPresent?"online":"absent"):"unknown",diagnostics.fileCount,diagnostics.lastError,(unsigned long)diagnostics.recoveries,diagnostics.lastTrack);
  if(ready) {enqueue(SD_FILE_COUNT,0);enqueue(0x42,0);}
}
void audioTest() {
  audioPrintDiagnostics();
  if(!soundEnabled||!volume()) {Serial.println("DFPLAYER: test muted; enable Sound and raise Volume first");return;}
  if(!ready) {testPending=true;Serial.println("DFPLAYER: test queued until SD initialization completes");return;}
  play(105,1250,true);
  enqueue(SD_FILE_COUNT,0);enqueue(0x42,0);
}
