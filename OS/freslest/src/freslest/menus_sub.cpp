#include "apps.h"
#include "../../gfx.h"
#include "settings.h"

// ===== Apps menu =====
static const char* APPS_ITEMS[] = { "Calculator", "Note", "Paint", "Clock", "Camera", "Files", "Browser", "Settings" };
static constexpr int APPS_COUNT = sizeof(APPS_ITEMS)/sizeof(APPS_ITEMS[0]);
static int appsIndex = 0;
static int appsTop = 0;
static int appsVisible(){return (tft.height()-78)/34;}
static void appsScroll(){if(appsIndex<appsTop)appsTop=appsIndex;if(appsIndex>=appsTop+appsVisible())appsTop=appsIndex-appsVisible()+1;}

static void drawAppsRow(int i, bool selected) {
  if(i<appsTop||i>=appsTop+appsVisible())return;
  int y = 54 + (i-appsTop) * 34;
  tft.fillRect(18, y - 2, 444, 34, TH().bg);

  if (selected) {
    tft.fillRoundRect(20, y, 440, 30, 8, TH().selbg);
    tft.drawRoundRect(20, y, 440, 30, 8, TH().accent);
    tft.setTextColor(TH().text, TH().selbg);
    tft.setCursor(40, y + 8);
    tft.setTextSize(2);
    tft.print("> ");
    tft.print(APPS_ITEMS[i]);
  } else {
    tft.setTextColor(TH().dim, TH().bg);
    tft.setCursor(40, y + 8);
    tft.setTextSize(2);
    tft.print("  ");
    tft.print(APPS_ITEMS[i]);
  }
}

void drawAppsMenuFull() {
  appsScroll();
  tft.fillScreen(TH().bg);
  drawHeader("Apps");
  drawFooter("UP/DOWN Move  OK Enter", "BACK Menu");
  clearBody();
  for (int i=0;i<APPS_COUNT;i++) drawAppsRow(i, i==appsIndex);
}

void drawAppsMenuSelectionOnly(int oldIdx, int newIdx) {
  int oldTop=appsTop;appsScroll();if(oldTop!=appsTop){drawAppsMenuFull();return;}
  drawAppsRow(oldIdx,false);
  drawAppsRow(newIdx,true);
  drawFooter("UP/DOWN Move  OK Enter", "BACK Menu");
}

static AppId appsMenuToApp(int idx){
  switch(idx){
    case 0: return APP_CALC;
    case 1: return APP_NOTE;
    case 2: return APP_PAINT;
    case 3: return APP_CLOCK;
    case 4: return APP_CAMERA;
    case 5: return APP_FILES;
    case 6: return APP_BROWSER;
    case 7: return APP_SETTINGS;
  }
  return APP_APPS;
}

// ===== Game menu =====
static const char* GAME_ITEMS[] = { "Dino", "Tetris", "Snake", "Mines", "2048", "Doom", "NES Library" };
static constexpr int GAME_COUNT = sizeof(GAME_ITEMS)/sizeof(GAME_ITEMS[0]);
static int gameIndex = 0;

static void drawGameRow(int i, bool selected) {
  // Fullscreen is inside NES Library.
  int y = 50 + i * 30;
  tft.fillRect(18, y - 2, 444, 30, TH().bg);

  if (selected) {
    tft.fillRoundRect(20, y, 440, 28, 8, TH().selbg);
    tft.drawRoundRect(20, y, 440, 28, 8, TH().accent);
    tft.setTextColor(TH().text, TH().selbg);
    tft.setCursor(40, y + 6);
    tft.setTextSize(2);
    tft.print("> ");
    tft.print(GAME_ITEMS[i]);
  } else {
    tft.setTextColor(TH().dim, TH().bg);
    tft.setCursor(40, y + 6);
    tft.setTextSize(2);
    tft.print("  ");
    tft.print(GAME_ITEMS[i]);
  }
}

void drawGameMenuFull() {
  tft.fillScreen(TH().bg);
  drawHeader("Game");
  drawFooter("UP/DOWN Move  OK Start", "BACK Menu");
  clearBody();
  for (int i=0;i<GAME_COUNT;i++) drawGameRow(i, i==gameIndex);
}

void drawGameMenuSelectionOnly(int oldIdx, int newIdx) {
  drawGameRow(oldIdx,false);
  drawGameRow(newIdx,true);
  drawFooter("UP/DOWN Move  OK Start", "BACK Menu");
}

static AppId gameMenuToApp(int idx){
  switch(idx){
    case 0: return APP_DINO;
    case 1: return APP_TETRIS;
    case 2: return APP_SNAKE;
    case 3: return APP_MINES;
    case 4: return APP_2048;
    case 5: return APP_DOOM;
    case 6: return APP_NES_LIBRARY;
  }
  return APP_GAME_MENU;
}

// Expose indices to apps.cpp
int& __appsIndex(){ return appsIndex; }
int  __appsCount(){ return APPS_COUNT; }
int __appsMenuRowAt(int y){int n=(y-54)/34;return y>=54&&n<appsVisible()&&n+appsTop<APPS_COUNT?n+appsTop:-1;}
AppId __appsMenuToApp(int idx){ return appsMenuToApp(idx); }

int& __gameIndex(){ return gameIndex; }
int  __gameCount(){ return GAME_COUNT; }
AppId __gameMenuToApp(int idx){ return gameMenuToApp(idx); }
