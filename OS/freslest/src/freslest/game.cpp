#include "game.h"
#include "../../gfx.h"
#include "settings.h"
#include "apps.h"   // for footer/header helpers
#include "audio_manager.h"
static bool gameOverSoundPlayed = false;
DinoGame game;

void DinoGame::reset() {
  running = true;
  dinoY = groundY;
  v = 0;
  obsX = 480;
  score = 0;
  lastTick = millis();
  prevDinoY = dinoY;
  prevObsX  = obsX;
  prevScore = -1;
}

void DinoGame::jump() {
  if (!running) { reset(); drawStatic(); return; }

  if (dinoY >= groundY) {
    v = -14;
    audioDino();   
  }
}

void DinoGame::drawStatic() {
  tft.fillScreen(TH().bg);
  drawHeader("Game: Dino");
  drawFooter("OK Jump/Start", "BACK Menu");
  clearBody();

  tft.drawLine(0, groundY + 28, 479, groundY + 28, TH().dim);
  prevScore = -1;
}

void DinoGame::updatePhysics() {
  uint32_t now = millis();
  if (now - lastTick < 28) return;
  lastTick = now;

  v += 1;
  dinoY += v;
  if (dinoY > groundY) { dinoY = groundY; v = 0; }

  obsX -= 5;
  if (obsX < -30) { obsX = 480 + random(0, 140); score++; }

  int dX = 70, dW = 26, dH = 26;
  int oY = groundY + 6, oW = 14, oH = 20;

  bool hit = !(dX + dW < obsX || obsX + oW < dX ||
               dinoY + dH < oY || oY + oH < dinoY);

  if (hit) running = false;
}

void DinoGame::drawFramePartial() {
  // erase previous dino
  tft.fillRect(70, prevDinoY, 26, 26, TH().bg);
  // erase previous obstacle
  tft.fillRect(prevObsX, groundY + 6, 14, 20, TH().bg);

  // redraw ground line
  tft.drawLine(0, groundY + 28, 479, groundY + 28, TH().dim);

  // draw current dino & obstacle
  tft.fillRoundRect(70, dinoY, 26, 26, 6, TH().accent);
  tft.fillRect(obsX, groundY + 6, 14, 20, TH().text);

  // score update
  if (score != prevScore) {
    tft.fillRect(340, 60, 130, 30, TH().bg);
    tft.setTextColor(TH().text, TH().bg);
    tft.setTextSize(2);
    tft.setCursor(360, 70);
    tft.print(score);
    prevScore = score;
  }

  // game over overlay
if (!running) {
  tft.fillRect(110, 120, 260, 90, TH().bg);
  tft.setTextColor(TH().text, TH().bg);
  tft.setTextSize(2);
  tft.setCursor(140, 150);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setCursor(150, 180);
  tft.print("OK to restart");

  if (!gameOverSoundPlayed) {
    audioGameOver();
    gameOverSoundPlayed = true;
  }
}

  prevDinoY = dinoY;
  prevObsX  = obsX;
}
