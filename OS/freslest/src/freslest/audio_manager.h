#pragma once
#include <Arduino.h>
void audioInit();
void audioUpdate(uint32_t now);
void audioStop();
void audioBootMelody();
void audioClick();
void audioDino();
void audioSnake();
void audioTetris();
void audioBoot();
void audioGame();
void audioGameOver();
void audioWin();
void soundApply();
void handleGlobalClick();

struct AudioDiagnostics {
  bool ready=false,replySeen=false,cardKnown=false,cardPresent=false;
  uint16_t fileCount=0,lastError=0,lastTrack=0;
  uint32_t txPackets=0,rxPackets=0,txFailures=0,invalidPackets=0,recoveries=0;
};
AudioDiagnostics audioGetDiagnostics();
void audioPrintDiagnostics();
void audioTest();
