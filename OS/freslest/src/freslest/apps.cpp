#include "test_3d.h"
#include "apps.h"
#include "ui_layout.h"
#include "../../camera.h"
#include "../../gfx.h"
#include "../../battery.h"
#include "../../input.h"
#include "settings.h"
#include "audio_manager.h"
#include "game.h"
#include "nes_library.h"
#include "app_doom.h"
#include "system_status.h"
#include "nes_usb.h"
#include "freslest_update_launcher.h"
#include "install_notice.h"
#include "task_hold.h"
#include "app_browser.h"
#include "text_entry.h"
#include "../../usb_input.h"
#include <freslest_hid.h>

#include <algorithm>
#include <esp_app_desc.h>
// ---- Test menu helpers ----
static int& testIdx();
static int testCount();

// ===== Test Menu forward declarations =====
void drawTestMenuFull();
void drawTestMenuSelectionOnly(int old, int cur);

AppId app = APP_MENU;

// ======================= MENU ITEMS =======================
static const char* MENU_ITEMS[] = { "Apps", "Settings", "System Info", "Test", "Game", "Freslest Update" };
static constexpr int MENU_COUNT = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
static int menuIndex = 0;

// ======================= TASK SWITCHER =======================
static constexpr uint32_t OK_HOLD_MS = 650;
static constexpr int MAX_TASKS = 5;

static AppId tasks[MAX_TASKS];
static int taskCount = 0;
static int taskSel = 0;
static bool taskView = false;
static bool taskInputConsumed=false;
static TaskHold taskHold;

// ======================= FPS =======================
uint32_t fpsValue = 0;
static uint32_t fpsLast = 0;
static uint32_t fpsCount = 0;
static bool footerDirty = true;

void fpsInit() {
  fpsLast = millis();
  fpsCount = 0;
  fpsValue = 0;
  footerDirty = true;
}

void fpsTick(uint32_t now) {
  fpsCount++;
  if (now - fpsLast >= 1000) {
    fpsValue = fpsCount;
    fpsCount = 0;
    fpsLast = now;
    footerDirty = true;

    if (!taskView && (app == APP_MENU || app == APP_APPS || app == APP_SETTINGS || app == APP_SYSINFO || app == APP_TEST)) {
      if (app == APP_MENU)     drawFooter("UP/DOWN Move  OK Enter", "HOLD OK Tasks");
      if (app == APP_APPS)     drawFooter("UP/DOWN Move  OK Enter", "BACK Menu");
      if (app == APP_SETTINGS) drawFooter("L/R Bright(UI) OK Theme", "BACK Save+Menu");
      if (app == APP_SYSINFO)  drawFooter("OK Refresh", "BACK Menu");
      if (app == APP_TEST)     drawFooter("Press buttons", "BACK Menu");
    }
  }
}

// ======================= TASK HELPERS =======================
static bool isInTasksLocal(AppId a) {
  for (int i = 0; i < taskCount; i++) if (tasks[i] == a) return true;
  return false;
}
static void addTaskLocal(AppId a) {
  if (a == APP_MENU) return;
  if (isInTasksLocal(a)) return;
  if (taskCount < MAX_TASKS) tasks[taskCount++] = a;
}
static void removeTaskAt(int idx) {
  if (idx < 0 || idx >= taskCount) return;
  for (int i = idx; i < taskCount - 1; i++) tasks[i] = tasks[i + 1];
  taskCount--;
  if (taskSel >= taskCount) taskSel = std::max(0, taskCount - 1);
}

// ======================= DRAW HELPERS =======================
static char headerTitle[35]="Main Menu";
static void drawHeaderSubtitle() {
  const char* notice=installNoticeText();
  int y=uiCompact(tft.width())?22:24;
  tft.fillRect(0,y,232,uiCompact(tft.width())?14:20,TH().panel);
  tft.setTextSize(1);tft.setTextColor(notice?TH().accent:TH().dim,TH().panel);
  tft.setCursor(uiCompact(tft.width())?8:10,uiCompact(tft.width())?24:28);tft.print(notice?notice:headerTitle);
}
void appInstallNoticeUpdate(uint32_t now,bool startupReady) {
  bool headerVisible=app!=APP_CAMERA&&app!=APP_FRESLEST_UPDATE&&!(app==APP_NES_PLAYER&&nesPlayerFullscreen());
  if(installNoticeUpdate(now,startupReady,headerVisible)&&headerVisible)drawHeaderSubtitle();
}
void drawHeader(const char* title) {
  tft.fillRect(0,0,tft.width(),uiHeaderHeight(tft.width()),TH().panel);
  tft.setTextColor(TH().text,TH().panel);
  tft.setTextSize(2);tft.setCursor(10,4);tft.print("FRESLEST");
  snprintf(headerTitle,sizeof(headerTitle),"%.34s",title);
  drawHeaderSubtitle();
  systemStatusDraw(true);drawBatteryTopBar(true);
}

void drawFooter(const char* leftHint, const char* rightHint) {
  const int y=tft.height()-24,w=tft.width();
  tft.fillRect(0,y,w,24,TH().panel);tft.setTextColor(TH().text,TH().panel);tft.setTextSize(1);
  if(uiCompact(w)){
    tft.setCursor(8,y+2);tft.printf("%.50s",leftHint);
    tft.setCursor(8,y+14);tft.printf("FPS:%lu",(unsigned long)fpsValue);
    int n=std::min(26,(int)strlen(rightHint));tft.setCursor(w-8-n*6,y+14);tft.printf("%.*s",n,rightHint);
  }else{
    tft.setCursor(10,y+6);tft.print(leftHint);tft.setCursor(200,y+6);tft.print("FPS:");tft.print(fpsValue);
    tft.setCursor(330,y+6);tft.print(rightHint);
  }
  footerDirty=false;
}
void clearBody(){int y=uiHeaderHeight(tft.width());tft.fillRect(0,y,tft.width(),tft.height()-y-24,TH().bg);}

// ======================= BOOT =======================
void bootScreen() {

  delay(0);
}

// ======================= MENU (partial redraw) =======================
static constexpr int MENU_TOPY = 54;
static constexpr int MENU_ITEMH = 39;

static void drawMenuRow(int i,bool selected){
 int w=tft.width(),row=uiMenuRowHeight(w,MENU_ITEMH),x=uiMenuLeft(w),y=uiMenuTop(w)+i*row;
 tft.fillRect(x-2,y-2,w-2*x+4,row,TH().bg);tft.setTextSize(2);
 if(selected){tft.fillRoundRect(x,y,w-2*x,row-3,6,TH().selbg);tft.drawRoundRect(x,y,w-2*x,row-3,6,TH().accent);}
 tft.setTextColor(selected?TH().text:TH().dim,selected?TH().selbg:TH().bg);
 tft.setCursor(uiMenuTextX(w),y+uiMenuTextY(w,9));tft.print(selected?"> ":"  ");tft.print(MENU_ITEMS[i]);
}

void drawMenuFull() {
  gfxSetCompactUi(true);
  tft.fillScreen(TH().bg);
  drawHeader("Main Menu");
  drawFooter("UP/DOWN Move  OK Enter", "HOLD OK Tasks");
  for (int i = 0; i < MENU_COUNT; i++) drawMenuRow(i, i == menuIndex);
}

void drawMenuSelectionOnly(int oldIdx, int newIdx) {
  drawMenuRow(oldIdx, false);
  drawMenuRow(newIdx, true);
  if (footerDirty) drawFooter("UP/DOWN Move  OK Enter", "HOLD OK Tasks");
}


static const char* appName(AppId a) {
  switch(a){
    case APP_APPS: return "Apps";
    case APP_SETTINGS: return "Settings";
    case APP_SYSINFO: return "System";
    case APP_TEST: return "Test";
    case APP_GAME_MENU: return "Game";
    case APP_CAMERA: return "Camera";
    case APP_CALC: return "Calc";
    case APP_NOTE: return "Note";
    case APP_FILES: return "Files";
    case APP_BROWSER: return "Browser";
    case APP_PAINT: return "Paint";
    case APP_CLOCK: return "Clock";
    case APP_SET_DISPLAY: return "Display";
    case APP_SET_BRIGHTNESS: return "Bright";
    case APP_SET_THEME: return "Theme";
    case APP_SET_WIFI: return "WiFi";
    case APP_SET_BT: return "BT";
    case APP_SET_FLASHRESET: return "Reset";
    case APP_DINO: return "Dino";
    case APP_TETRIS: return "Tetris";
    case APP_SNAKE: return "Snake";
    case APP_MINES: return "Mines";
    case APP_2048: return "2048";
    case APP_DOOM: return "Doom";
    case APP_NES_LIBRARY: return "NES Lib";
    case APP_NES_PLAYER: return "NES";
    case APP_FRESLEST_UPDATE: return "Update";
    default: return "App";
  }
}

// ======================= TASK SWITCHER UI =======================
static void drawTaskSwitcherFull() {
  gfxSetCompactUi(true);
  tft.fillScreen(TH().bg);
  drawHeader("Task Switcher");
  drawFooter("L/R Select  OK Switch", "UP Close  BACK Exit");

  tft.setTextSize(2);

  if (taskCount == 0) {
    tft.setTextColor(TH().text, TH().bg);
    tft.setCursor(60, 140);
    tft.print("No apps running");
    tft.setTextSize(1);
    tft.setCursor(60, 170);
    tft.print("Open an app from menu.");
    return;
  }

  // 3 apps per page
  const int PER_PAGE = uiCompact(tft.width())?2:3;
  int totalPages = (taskCount + PER_PAGE - 1) / PER_PAGE;
  int page = taskSel / PER_PAGE;
  int start = page * PER_PAGE;
  int end = start + PER_PAGE;
  if (end > taskCount) end = taskCount;

  int cardW = 140, cardH = uiCompact(tft.width())?76:90;
  int gap = uiCompact(tft.width())?12:10;
  int shown = end - start;
  int totalW = shown * cardW + (shown - 1) * gap;
  int startX = (tft.width() - totalW) / 2;
  int y = uiCompact(tft.width())?92:110;

  for (int i = start; i < end; i++) {
    int local = i - start;
    int x = startX + local * (cardW + gap);
    bool sel = (i == taskSel);
    uint16_t fill = sel ? TH().selbg : TH().panel;

    tft.fillRoundRect(x, y, cardW, cardH, 14, fill);
    tft.drawRoundRect(x, y, cardW, cardH, 14, sel ? TH().accent : TH().dim);

    tft.setTextColor(TH().text, fill);
    tft.setCursor(x + 12, y + 18);
    tft.setTextSize(2);
    tft.print(appName(tasks[i]));

    tft.setTextSize(1);
    tft.setCursor(x + 12, y + 52);
    tft.print("OK to switch");
  }

  // page dots
  tft.setTextSize(2);
  int dotsY = uiCompact(tft.width())?190:250;
  int dotsW = totalPages * 14;
  int dotsX = (tft.width() - dotsW) / 2;
  for (int p = 0; p < totalPages; p++) {
    uint16_t c = (p == page) ? TH().accent : TH().dim;
    tft.fillCircle(dotsX + p * 14, dotsY, 4, c);
  }

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(20,uiCompact(tft.width())?202:270);
  tft.print("Tip: UP closes selected task.");
}

void taskSwitcherHandle(uint32_t now) {
  taskInputConsumed=false;
  if (textEntryActive()||app==APP_NES_PLAYER||app==APP_USB_TRANSFER||app==APP_FRESLEST_UPDATE) {taskHold.blockUntilRelease();return;}
  bool open=taskHold.update(bOk.down(),now,OK_HOLD_MS)||usbInputTasks()||(app==APP_MENU&&usbInputHit(tft.width()-150,tft.height()-24,150,24));
  if (!taskView && open) {
      if(app==APP_BROWSER)browserExit();
      taskView = true;
      taskSel = std::min(taskSel, std::max(0, taskCount - 1));
      drawTaskSwitcherFull(); audioClick();
      taskInputConsumed=true;
      return;
  }

  if (!taskView) return;

  bool changed = false;
  if(usbPointer.click&&taskCount>0){
    int count=uiCompact(tft.width())?2:3,gap=uiCompact(tft.width())?12:10;
    int first=(taskSel/count)*count,end=std::min(first+count,taskCount),shown=end-first;
    int x=(tft.width()-(shown*140+(shown-1)*gap))/2;
    for(int i=first;i<end;i++)if(usbInputHit(x+(i-first)*(140+gap),uiCompact(tft.width())?92:110,140,uiCompact(tft.width())?76:90)){taskSel=i;bOk.pressedEdge=true;taskHold.waitingRelease=false;break;}
  }
  if (bLeft.pressed() && taskCount > 0)  { taskSel = (taskSel - 1 + taskCount) % taskCount; changed = true; audioClick(); }
  if (bRight.pressed() && taskCount > 0) { taskSel = (taskSel + 1) % taskCount; changed = true; audioClick(); }

  if (bUp.pressed() && taskCount > 0) {
    AppId closing = tasks[taskSel];
    removeTaskAt(taskSel);
    if (closing==APP_NES_PLAYER)nesPlayerExit();
    if (closing==APP_NOTE)noteExit();
    if (closing==APP_BROWSER)browserExit();
    if (app == closing) app = APP_MENU;
    changed = true; audioClick();
  }

  if (bOk.pressed() && !taskHold.waitingRelease && taskCount > 0) {
    AppId target = tasks[taskSel];
    audioClick();
    taskView = false;
    taskHold.blockUntilRelease();taskInputConsumed=true;
    if (target == APP_MENU) drawMenuFull();
    else enterApp(target);
    return;
  }

  if (bBack.pressed()) {
    audioClick();
    taskView = false;
    taskHold.blockUntilRelease();taskInputConsumed=true;
    if (app == APP_MENU) drawMenuFull();
    else enterApp(app);
    return;
  }

  if (changed) { drawTaskSwitcherFull(); audioClick(); }
}

// ======================= APPS =======================
void drawApps() {
  drawAppsMenuFull();
}

void drawSettings() {
  drawSettingsMenuFull();
}

void drawSysInfo() {
  tft.fillScreen(TH().bg);
  drawHeader("System Info");
  drawFooter("OK Refresh", "BACK Menu");
  clearBody();

  tft.setTextColor(TH().text, TH().bg);
  tft.setTextSize(1);

  int y = uiCompact(tft.width())?54:70;
  const int line=uiCompact(tft.width())?14:18;
  tft.setCursor(20,y);tft.printf("Freslest OS: %s",esp_app_get_description()->version);y+=line;
  tft.setCursor(20, y); tft.printf("CPU: %u MHz", (unsigned)getCpuFrequencyMhz()); y += line;
  tft.setCursor(20, y); tft.printf("Flash: %u bytes", (unsigned)ESP.getFlashChipSize()); y += line;
  tft.setCursor(20, y); tft.printf("Heap Free: %u bytes", (unsigned)ESP.getFreeHeap()); y += line;

#if defined(BOARD_HAS_PSRAM)
  tft.setCursor(20, y); tft.printf("PSRAM Found: %s", psramFound() ? "YES" : "NO"); y += line;
  tft.setCursor(20, y); tft.printf("PSRAM Free: %u bytes", (unsigned)ESP.getFreePsram()); y += line;
#else
  tft.setCursor(20, y); tft.print("PSRAM: (board flag off)"); y += line;
#endif

  tft.setCursor(20, y); tft.printf("Theme: %s", darkMode ? "Dark" : "Light"); y += line;
  tft.setCursor(20, y); tft.printf("Brightness(UI): %d", brightnessUI); y += line;
  freslest_hid_snapshot_t usb{};freslest_hid_peek(&usb);
  tft.setCursor(20,y);tft.printf("Input: %u keyboard(s), %u mouse/mice",usb.keyboards,usb.mice);y+=line;
  tft.setCursor(20,y);tft.printf("%.48s",freslest_hid_status());
}

void drawTestBase() {
  tft.fillScreen(TH().bg);
  drawHeader("Test");
  drawFooter("Press buttons", "BACK Menu");
  clearBody();

  tft.setTextColor(TH().text, TH().bg);
  tft.setTextSize(2);
  tft.setCursor(20,uiCompact(tft.width())?54:70);
  tft.print("Buttons live:");

  tft.fillRoundRect(uiCompact(tft.width())?12:20,uiCompact(tft.width())?94:120,tft.width()-(uiCompact(tft.width())?24:40),uiCompact(tft.width())?108:150,8,TH().panel);
}

void drawTestStatesOnly() {
  tft.fillRect(uiCompact(tft.width())?16:30,uiCompact(tft.width())?100:130,tft.width()-(uiCompact(tft.width())?32:60),uiCompact(tft.width())?96:130,TH().panel);
  tft.setTextColor(TH().text, TH().panel);
  tft.setTextSize(2);
  tft.setCursor(uiCompact(tft.width())?24:40,uiCompact(tft.width())?104:140);
  tft.printf("UP:%d  DOWN:%d", bUp.down(), bDown.down());
  tft.setCursor(uiCompact(tft.width())?24:40,uiCompact(tft.width())?132:170);
  tft.printf("LT:%d  RT:%d", bLeft.down(), bRight.down());
  tft.setCursor(uiCompact(tft.width())?24:40,uiCompact(tft.width())?160:200);
  tft.printf("OK:%d  BK:%d", bOk.down(), bBack.down());
  tft.setTextSize(1);
  tft.setCursor(uiCompact(tft.width())?24:40,uiCompact(tft.width())?188:235);
  tft.printf("FPS:%u", (unsigned)fpsValue);
}

// ======================= NAV =======================
AppId menuToApp(int idx) {
  switch (idx) {
    case 0: return APP_APPS;
    case 1: return APP_SETTINGS;
    case 2: return APP_SYSINFO;
    case 3: return APP_TEST;
    case 4: return APP_GAME_MENU;
    case 5: return APP_FRESLEST_UPDATE;
    default: return APP_MENU;
  }
}

void enterApp(AppId next) {
  // Normalize: main Settings opens settings menu; main Game opens game menu
  if (next == APP_SETTINGS) next = APP_SETTINGS;
  if (next == APP_GAME) next = APP_GAME_MENU;

  if(app==APP_NES_PLAYER && next!=APP_NES_PLAYER)nesPlayerExit();
  if((app==APP_USB_TRANSFER||app==APP_NES_LIBRARY)&&next!=APP_USB_TRANSFER&&next!=APP_NES_LIBRARY)nesUsbExit();
  if(app==APP_NOTE && next!=APP_NOTE)noteExit();
  if(app==APP_BROWSER && next!=APP_BROWSER)browserExit();
  if(app==APP_SET_WIFI && next!=APP_SET_WIFI)setWifiExit();
  gfxSetCompactUi(next==APP_MENU||next==APP_APPS||next==APP_CALC||next==APP_NOTE||next==APP_CLOCK||next==APP_PAINT||next==APP_CAMERA||next==APP_FILES||next==APP_BROWSER||next==APP_SETTINGS||next==APP_GAME_MENU||next==APP_SYSINFO||next==APP_TEST||next==APP_TEST_BUTTONS||next==APP_SET_BRIGHTNESS||next==APP_SET_THEME||next==APP_SET_WIFI||next==APP_SET_BT||next==APP_SET_SOUND||next==APP_SET_FLASHRESET||next==APP_SET_DISPLAY);
  app = next;
  addTaskLocal(app);
  footerDirty = true;

  switch (app) {
    case APP_APPS:        drawAppsMenuFull(); break;
    case APP_SETTINGS:    drawSettingsMenuFull(); break;
    case APP_SYSINFO:     drawSysInfo(); break;
    case APP_CAMERA:
      cameraEnter();
      break;
    case APP_TEST:        drawTestMenuFull(); break;

    case APP_TEST_BUTTONS:{ drawTestBase(); drawTestStatesOnly(); } break;
    case APP_TEST_3D:     test3dEnter(); break;

    case APP_GAME_MENU:   drawGameMenuFull(); break;

    case APP_CALC:        calcEnter(); break;
    case APP_NOTE:        noteEnter(); break;
    case APP_FILES:       filesEnter(); break;
    case APP_BROWSER:     browserEnter(); break;
    case APP_PAINT:       paintEnter(); break;
    case APP_CLOCK:       clockEnter(); break;

    case APP_SET_DISPLAY: setDisplayEnter(); break;
    case APP_SET_BRIGHTNESS: setBrightnessEnter(); break;
    case APP_SET_THEME:      setThemeEnter(); break;
    case APP_SET_WIFI:       setWifiEnter(); break;
    case APP_SET_BT:         setBtEnter(); break;
    case APP_SET_SOUND:      setSoundEnter(); break;
    case APP_SET_FLASHRESET: setFlashResetEnter(); break;

    case APP_DINO:        dinoEnter(); break;
    case APP_DOOM: doomEnter(); break;
    case APP_TETRIS:      tetrisEnter(); break;
    case APP_SNAKE:       snakeEnter(); break;
    case APP_MINES:       minesEnter(); break;
    case APP_2048:        g2048Enter(); break;
    case APP_NES_LIBRARY: nesLibraryEnter(); break;
    case APP_NES_PLAYER:  nesPlayerEnter(); break;
    case APP_USB_TRANSFER: nesUsbEnter(); break;
    case APP_FRESLEST_UPDATE: freslestUpdateEnter(); break;

    default: break;
  }
}

void backToMenu() {
  if(app==APP_BROWSER)browserExit();
  if(app==APP_USB_TRANSFER||app==APP_NES_LIBRARY)nesUsbExit();
  // cleanup for pages that keep services running
  if (app == APP_SET_WIFI) setWifiExit();
  if (app == APP_SET_BT)   setBtExit();

  app = APP_MENU;
  footerDirty = true;
  drawMenuFull();
}

// ======================= MAIN APP LOOP =======================
void appLoop(uint32_t now) {
  if(taskView||taskInputConsumed)return;
  if(textEntryActive()){textEntryLoop();return;}
  int cnt = testCount();
  if(app!=APP_CAMERA&&!(app==APP_NES_PLAYER&&nesPlayerFullscreen())) {systemStatusDraw();drawBatteryTopBar();}
  if(app==APP_USB_TRANSFER) {nesUsbLoop(now);return;}
  if(app==APP_FRESLEST_UPDATE) {freslestUpdateLoop(now);return;}
  if(usbInputHit(tft.width()-150,tft.height()-24,150,24))bBack.pressedEdge=true;
  if(app==APP_FILES){filesLoop(now);return;}
  if(app==APP_SET_WIFI){setWifiLoop(now);return;}
  if(app==APP_BROWSER){browserLoop(now);return;}
  int mouseRow=-1;
  if(app==APP_MENU&&usbInputHit(uiMenuLeft(tft.width()),uiMenuTop(tft.width()),tft.width()-2*uiMenuLeft(tft.width()),MENU_COUNT*uiMenuRowHeight(tft.width(),39)))mouseRow=(usbPointer.y-uiMenuTop(tft.width()))/uiMenuRowHeight(tft.width(),39);
  if(app==APP_APPS&&usbInputHit(uiMenuLeft(tft.width()),uiMenuTop(tft.width()),tft.width()-2*uiMenuLeft(tft.width()),tft.height()-uiMenuTop(tft.width())-24))mouseRow=__appsMenuRowAt(usbPointer.y);
  if(app==APP_SETTINGS&&usbInputHit(uiMenuLeft(tft.width()),uiMenuTop(tft.width()),tft.width()-2*uiMenuLeft(tft.width()),__settingsCount()*uiMenuRowHeight(tft.width(),34)))mouseRow=(usbPointer.y-uiMenuTop(tft.width()))/uiMenuRowHeight(tft.width(),34);
  if(app==APP_GAME_MENU&&usbInputHit(uiMenuLeft(tft.width()),uiMenuTop(tft.width(),50),tft.width()-2*uiMenuLeft(tft.width()),__gameCount()*uiMenuRowHeight(tft.width(),30)))mouseRow=(usbPointer.y-uiMenuTop(tft.width(),50))/uiMenuRowHeight(tft.width(),30);
  if(app==APP_TEST&&usbInputHit(uiMenuLeft(tft.width()),uiMenuTop(tft.width()),tft.width()-2*uiMenuLeft(tft.width()),2*uiMenuRowHeight(tft.width(),39)))mouseRow=(usbPointer.y-uiMenuTop(tft.width()))/uiMenuRowHeight(tft.width(),39);
  if(mouseRow>=0){
    if(app==APP_MENU)menuIndex=mouseRow;
    if(app==APP_APPS)__appsIndex()=mouseRow;
    if(app==APP_SETTINGS)__settingsIndex()=mouseRow;
    if(app==APP_GAME_MENU)__gameIndex()=mouseRow;
    if(app==APP_TEST)testIdx()=mouseRow;
    bOk.pressedEdge=true;
  }

  // BACK navigation tree:
  if (bBack.pressed()) {
    if(app==APP_CLOCK && clockCancelEdit())return;
    if(app==APP_NES_PLAYER)nesPlayerExit();
    // exit services if needed
    if (app == APP_SET_WIFI) setWifiExit();
    if (app == APP_SET_BT)   setBtExit();

    // From sub-apps -> parent menu
    if (app == APP_CALC || app == APP_NOTE || app == APP_PAINT || app == APP_CLOCK) {
      enterApp(APP_APPS);
      return;
    }
    if (app == APP_SET_BRIGHTNESS || app == APP_SET_THEME || app == APP_SET_WIFI || app == APP_SET_BT || app == APP_SET_SOUND || app == APP_SET_FLASHRESET || app == APP_SET_DISPLAY) {
      settingsSave();
      enterApp(APP_SETTINGS);
      return;
    }
    if (app == APP_NES_PLAYER) {
      enterApp(APP_NES_LIBRARY);
      return;
    }
    if (app == APP_DINO || app == APP_TETRIS || app == APP_SNAKE || app == APP_MINES || app == APP_2048 || app == APP_DOOM || app == APP_NES_LIBRARY) {
      enterApp(APP_GAME_MENU);
      return;
    }

  if (app == APP_CAMERA) {
    Serial.println("EXIT CAMERA");

    // kamera bağlantısını kapat
    cameraExit();  // 👈 bunu ekle (aşağıda veriyorum)

    enterApp(APP_MENU);
    return;
  }

    // Parent pages -> main menu
    backToMenu();
    return;
  }

  // MAIN MENU
  if (app == APP_MENU) {
    
    if (bUp.pressed()) {
      int old = menuIndex;
      menuIndex = (menuIndex - 1 + MENU_COUNT) % MENU_COUNT;
      drawMenuSelectionOnly(old, menuIndex);
    }
    if (bDown.pressed()) {
      int old = menuIndex;
      menuIndex = (menuIndex + 1) % MENU_COUNT;
      drawMenuSelectionOnly(old, menuIndex);
    }
    if (bOk.pressed()) {
      enterApp(menuToApp(menuIndex));
    }
    return;
  }

  // APPS MENU
  if (app == APP_APPS) {
    int& idx = __appsIndex();
        if (bUp.pressed()) {
      int old = idx;
      idx--; if (idx < 0) idx = 0;
      drawAppsMenuSelectionOnly(old, idx);
    }
    if (bDown.pressed()) {
      int old = idx;
      idx = (idx+1)%__appsCount();
      drawAppsMenuSelectionOnly(old, idx);
    }
    if (bOk.pressed()) {
      enterApp(__appsMenuToApp(idx));
    }



    return;
  }

  // SETTINGS MENU
  if (app == APP_SETTINGS) {
    int& idx = __settingsIndex();
    if (bUp.pressed()) {
      int old = idx;
      idx--; if (idx < 0) idx = 0;
      drawSettingsMenuSelectionOnly(old, idx);
    }
    if (bDown.pressed()) {
      int old = idx;
      idx = (idx+1)%__settingsCount();
      drawSettingsMenuSelectionOnly(old, idx);
    }
    if (bOk.pressed()) {
      enterApp(settingsMenuToApp(idx));
    }



    return;
  }

  // GAME MENU
  if (app == APP_GAME_MENU) {
    int& idx = __gameIndex();
        if (bUp.pressed()) {
      int old = idx;
      idx--; if (idx < 0) idx = __gameCount() - 1;
      drawGameMenuSelectionOnly(old, idx);
    }
    if (bDown.pressed()) {
      int old = idx;
      idx = (idx + 1) % __gameCount();
      drawGameMenuSelectionOnly(old, idx);
    }
    if (bOk.pressed()) {
      enterApp(__gameMenuToApp(idx));
    }



    return;
  }

  // SYSINFO
  if (app == APP_SYSINFO) {
    if(usbInputHit(20,60,440,220))bOk.pressedEdge=true;
    if (bOk.pressed()) drawSysInfo();
    return;
  }

  // TEST
  

// Test pages
if (app == APP_TEST_BUTTONS) {
  if (bBack.pressed()) { enterApp(APP_TEST); return; }
  drawTestStatesOnly();
 return;
}
if (app == APP_TEST_3D) {
  test3dLoop(now);
  if (test3dWantsExit()) { test3dExit(); enterApp(APP_TEST); }
  return;
}

if (app == APP_TEST) {
    if (bBack.pressed()) { enterApp(APP_MENU); return; }

  if (bUp.pressed()) {
    int old = testIdx();
    testIdx() = (testIdx() - 1 + cnt) % cnt;
    drawTestMenuSelectionOnly(old, testIdx());
    handleGlobalClick();
  }
  if (bDown.pressed()) {
    int old = testIdx();
    testIdx() = (testIdx() + 1) % cnt;
    drawTestMenuSelectionOnly(old, testIdx());
    handleGlobalClick();
  }
  if (bOk.pressed()) {
    handleGlobalClick();
    if (testIdx() == 0) enterApp(APP_TEST_BUTTONS);
    else                enterApp(APP_TEST_3D);
  }
  return;
}


  // Sub-apps
  if (app == APP_CALC)  { calcLoop(now); return; }
  if (app == APP_NOTE)  { noteLoop(now); return; }
  if (app == APP_PAINT) { paintLoop(now); return; }
  if (app == APP_CLOCK) { clockLoop(now); return; }

  if(app==APP_SET_DISPLAY) {setDisplayLoop(now);return;}
  if(app==APP_SET_SOUND) {setSoundLoop(now);return;}
  // Settings pages
  if (app == APP_SET_BRIGHTNESS) { setBrightnessLoop(now); return; }
  if (app == APP_SET_THEME)      { setThemeLoop(now); return; }
  if (app == APP_SET_WIFI)       { setWifiLoop(now); return; }
  if (app == APP_SET_BT)         { setBtLoop(now); return; }
  if (app == APP_SET_FLASHRESET) { setFlashResetLoop(now); return; }

  if (app == APP_CAMERA) { cameraLoop(now); return; }

  // Games
  if (app == APP_DINO)   { dinoLoop(now); return; }
  if (app == APP_TETRIS) { tetrisLoop(now); return; }
  if (app == APP_SNAKE)  { snakeLoop(now); return; }
  if (app == APP_MINES)  { minesLoop(now); return; }
  if (app == APP_2048)   { g2048Loop(now); return; }
  if (app == APP_DOOM) { doomLoop(now); return; }
  if (app == APP_NES_LIBRARY) { nesLibraryLoop(now); return; }
  if (app == APP_NES_PLAYER)  { nesPlayerLoop(now); return; }
}

// --- Test submenu index ---
static int __test_idx = 0;
static int& __testIndex(){ return __test_idx; }
static int __testCount(){ return 2; }

void drawTestMenuFull(){
 tft.fillScreen(TH().bg);drawHeader("Test");drawFooter("UP/DOWN Move  OK Enter","BACK Main");testIdx()=0;
 drawTestMenuSelectionOnly(-1,0);drawTestMenuSelectionOnly(-1,1);drawTestMenuSelectionOnly(1,0);
}
void drawTestMenuSelectionOnly(int old,int cur){
 auto drawRow=[&](int i,bool selected){
  int w=tft.width(),row=uiMenuRowHeight(w,39),x=uiMenuLeft(w),y=uiMenuTop(w)+i*row;
  tft.fillRect(x-2,y-2,w-2*x+4,row,TH().bg);
  if(selected){tft.fillRoundRect(x,y,w-2*x,row-3,6,TH().selbg);tft.drawRoundRect(x,y,w-2*x,row-3,6,TH().accent);}
  tft.setTextSize(2);tft.setTextColor(selected?TH().text:TH().dim,selected?TH().selbg:TH().bg);
  tft.setCursor(uiMenuTextX(w),y+uiMenuTextY(w,9));tft.print(selected?"> ":"  ");tft.print(i==0?"Buttons Test":"3D Tester");
 };
 if(old>=0)drawRow(old,false);
 drawRow(cur,true);
}

// --- Test submenu index ---
static int __testMenuIdx = 0;
static int& testIdx(){ return __testMenuIdx; }
static int testCount(){ return 2; }


