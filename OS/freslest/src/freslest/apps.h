#pragma once
#include <Arduino.h>
#include "settings.h"

enum AppId : uint8_t {
  // Main
  APP_MENU = 0,
  APP_APPS,
  APP_SETTINGS,
  APP_SYSINFO,
  APP_TEST,
  APP_TEST_BUTTONS,
  APP_TEST_3D,
  APP_GAME,
  APP_CAMERA,

  // Apps submenu
  APP_CALC,
  APP_NOTE,
  APP_PAINT,
  APP_CLOCK,


  // Settings submenu
  APP_SET_BRIGHTNESS,
  APP_SET_THEME,
  APP_SET_WIFI,
  APP_SET_BT,
  APP_SET_SOUND,
  APP_SET_FLASHRESET,

  // Game submenu + games
  APP_GAME_MENU,
  APP_DINO,
  APP_TETRIS,
  APP_SNAKE,
  APP_MINES,
  APP_2048,
  APP_DOOM,
  APP_NES_LIBRARY,
  APP_NES_PLAYER,
  APP_USB_TRANSFER,
  APP_FRESLEST_UPDATE,
  APP_FILES,
  APP_SET_DISPLAY,
  APP_SET_TV,
  APP_SET_TV_COLOR,
  APP_BROWSER,
};

extern AppId app;

// ===== UI / COMMON =====
void drawHeader(const char* title);
void appInstallNoticeUpdate(uint32_t now,bool startupReady);
void drawFooter(const char* leftHint, const char* rightHint);
void clearBody();

// ===== FPS =====
extern uint32_t fpsValue;
void fpsInit();
void fpsTick(uint32_t now);

// ===== Menu =====
void bootScreen();
void drawMenuFull();
void drawMenuSelectionOnly(int oldIdx, int newIdx);

// ===== Task Switcher =====
void taskSwitcherHandle(uint32_t now);

// ===== Apps =====
void drawApps();
void drawSettings();
void drawSysInfo();
void drawTestBase();
void drawTestStatesOnly();

// ===== Navigation / Loop =====
AppId menuToApp(int idx);
void enterApp(AppId next);
void backToMenu();
void appLoop(uint32_t now);

// ===== Submenus =====
void drawAppsMenuFull();
void drawAppsMenuSelectionOnly(int oldIdx, int newIdx);
void drawGameMenuFull();
void drawGameMenuSelectionOnly(int oldIdx, int newIdx);
void drawSettingsMenuFull();
void drawSettingsMenuSelectionOnly(int oldIdx, int newIdx);

// ===== Apps =====
void calcEnter();
void calcLoop(uint32_t now);

void filesEnter();
void filesLoop(uint32_t now);
void noteExit();
void noteEnter();
void noteLoop(uint32_t now);

void paintEnter();
void paintLoop(uint32_t now);

bool clockCancelEdit();
void clockEnter();
void clockLoop(uint32_t now);

// ===== Settings =====
void setDisplayEnter();
void setDisplayLoop(uint32_t now);
void setTvEnter();
void setTvLoop(uint32_t now);
void setTvExit();
void setTvColorEnter();
void setTvColorLoop(uint32_t now);
void setTvColorExit();
void setBrightnessEnter();
void setBrightnessLoop(uint32_t now);

void setThemeEnter();
void setThemeLoop(uint32_t now);

void setWifiEnter();
void setWifiLoop(uint32_t now);

void setBtEnter();
void setBtLoop(uint32_t now);

void setSoundEnter();
void setSoundLoop(uint32_t now);

void setFlashResetEnter();
void setFlashResetLoop(uint32_t now);

// ===== Games =====
void dinoEnter();
void dinoLoop(uint32_t now);

void tetrisEnter();
void tetrisLoop(uint32_t now);

void snakeEnter();
void snakeLoop(uint32_t now);

void minesEnter();
void minesLoop(uint32_t now);

void g2048Enter();
void g2048Loop(uint32_t now);

// ===== NES =====
void nesLibraryEnter();
void nesLibraryLoop(uint32_t now);
void nesPlayerExit();
void nesPlayerEnter();
void nesPlayerLoop(uint32_t now);

// ===== internal helpers exposed across TUs =====
AppId settingsMenuToApp(int idx);
int& __settingsIndex();
int  __settingsCount();

// ===== Internal menu helpers =====
int& __appsIndex();
int  __appsCount();
int __appsMenuRowAt(int y);
AppId __appsMenuToApp(int idx);

int& __gameIndex();
int  __gameCount();
AppId __gameMenuToApp(int idx);

// ===== Cleanup on exit =====
void setWifiExit();
void setBtExit();
