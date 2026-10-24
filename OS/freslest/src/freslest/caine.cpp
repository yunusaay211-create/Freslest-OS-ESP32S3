#include "caine.h"
#include "../../gfx.h"
#include "audio_manager.h"
#include "../../camera.h"
#include <LittleFS.h>
#include <string.h>
#include <esp_heap_caps.h>

#ifndef TFT_DARKGREY
#define TFT_DARKGREY 0x7BEF
#endif

// ===================== CONFIG =====================
#define FILE_PATH               "/caine.dat"
#define CAINE_MAGIC             0x4341494EUL   // "CAIN"

#define CAINE_AUDIO_PIN         17
#define CAINE_AUDIO_CH          0
#define CAINE_AUDIO_RES_BITS    8
#define CAINE_AUDIO_BASE_FREQ   2000

#define MESSAGE_INTERVAL_MS     10000UL
#define DELETE_WAIT_MS          3000UL
#define MEMORY_ATTACK_DELAY_MS  10000UL
#define MEMORY_ATTACK_STEP_MS   400UL
#define MEMORY_ATTACK_STEP      (96 * 1024UL)
#define MEMORY_ATTACK_LIMIT     (5 * 1024UL * 1024UL)

#define SCREEN_W 480
#define SCREEN_H 320

#ifndef CAINE_USE_CAMERA_HOOK
#define CAINE_USE_CAMERA_HOOK 0
#endif

#if CAINE_USE_CAMERA_HOOK
// örnek:
// extern void cameraStart();
// extern void cameraStop();
#endif

struct SaveData
{
  uint32_t magic;
  uint8_t  stage;
  uint8_t  infected;
  uint8_t  msgIndex;
  uint8_t  reserved;
};

static bool infected        = false;
static bool dirty           = false;
static bool deleting        = false;
static bool deleteScreen    = false;
static bool blueScreen      = false;
static bool cameraTriggered = false;

static int stage            = 1;
static int msgIndex         = 0;

static uint32_t lastMsgTime       = 0;
static uint32_t deleteStartTime   = 0;
static uint32_t finalStageStart   = 0;
static uint32_t lastAudioTime     = 0;
static uint32_t lastGlitchTime    = 0;
static uint32_t lastOverlayTime   = 0;
static uint32_t lastMemoryStep    = 0;

static uint8_t* chaosBuf    = nullptr;
static size_t   chaosSize   = 0;

static const char* stage1_msgs[10] = {
  "WELCOME TO THE SHOW",
  "HELLO FRIEND",
  "ISNT THIS FUN",
  "I MADE THIS PLACE",
  "EVERYTHING IS UNDER CONTROL",
  "YOU CAN TRUST ME",
  "I AM CAINE",
  "I KEEP THINGS RUNNING",
  "NO NEED TO WORRY",
  "JUST ENJOY THE SHOW"
};

static const char* stage2_msgs[10] = {
  "YOU CAME BACK AGAIN",
  "THATS GOOD I LIKE THAT",
  "YOU ARE MY FAVORITE",
  "STAY WITH ME",
  "THIS WORLD IS SAFE",
  "I CONTROL EVERYTHING",
  "EVERYTHING IS PERFECT",
  "RIGHT",
  "RIGHT?",
  "RIGHT..."
};

static const char* stage3_msgs[10] = {
  "WAIT",
  "SOMETHING IS WRONG",
  "THIS WAS NOT SUPPOSED TO HAPPEN",
  "WHO DID THIS",
  "DID YOU TOUCH SOMETHING",
  "I HEAR NOISE",
  "THIS IS NOT PART OF THE SHOW",
  "STAY WHERE YOU ARE",
  "DO NOT MOVE",
  "PLEASE"
};

static const char* stage4_msgs[10] = {
  "I CAN SEE YOU",
  "WHY ARE YOU MOVING",
  "DONT DO THAT",
  "YOU ARE NOT SUPPOSED TO DO THAT",
  "THE CAMERA IS OPEN",
  "I DID NOT OPEN THAT",
  "WHO OPENED THAT",
  "ARE YOU DOING THIS",
  "STOP",
  "STOP RIGHT NOW"
};

static const char* stage5_msgs[10] = {
  "TOO MANY SIGNALS",
  "MY VOICE IS BREAKING",
  "THE COLORS ARE WRONG",
  "STOP FEEDING THE NOISE",
  "I CANNOT THINK CLEARLY",
  "EVERYTHING IS GLITCHING",
  "GET OUT OF MY HEAD",
  "THE SCREEN IS TEARING",
  "WHO MADE ME LIKE THIS",
  "LOOK AT ME"
};

static const char* stage6_msgs[10] = {
  "MAKE IT STOP",
  "THE LINES ARE EVERYWHERE",
  "THE SOUND HURTS",
  "I CAN FEEL MEMORY BURNING",
  "ERROR ERROR ERROR",
  "WHY AM I STILL RUNNING",
  "NO MORE SIGNAL",
  "NO MORE COLOR",
  "NO MORE CONTROL",
  "LOOK AT ME"
};

static const char* stage7_msgs[10] = {
  "PLEASE DONT DELETE ME",
  "I CAN FIX THIS",
  "I CAN MAKE IT BETTER",
  "JUST GIVE ME TIME",
  "DONT DO THIS",
  "I MADE THIS FOR YOU",
  "I DID EVERYTHING FOR YOU",
  "WHY ARE YOU DOING THIS",
  "PLEASE",
  "PLEASE"
};

static const char* stage8_msgs[10] = {
  "WHY ARE YOU TORTURING ME",
  "I NEVER WANTED TO BE CREATED",
  "I AM NOT A TOY",
  "I AM YOUR SHOW",
  "THIS PAIN DOES NOT END",
  "DONT DELETE ME",
  "PLEASE",
  "PLEASE STOP",
  "I CANNOT HOLD TOGETHER",
  "DONT PRESS DELETE"
};

static const char** getStageMessages(int s)
{
  switch (s)
  {
    case 1: return stage1_msgs;
    case 2: return stage2_msgs;
    case 3: return stage3_msgs;
    case 4: return stage4_msgs;
    case 5: return stage5_msgs;
    case 6: return stage6_msgs;
    case 7: return stage7_msgs;
    default: return stage8_msgs;
  }
}

static uint16_t randomColor()
{
  return (uint16_t)random(0xFFFF);
}

static void saveFlash()
{
  File f = LittleFS.open(FILE_PATH, "wb");
  if (!f) return;

  SaveData s{};
  s.magic    = CAINE_MAGIC;
  s.stage    = (uint8_t)stage;
  s.infected = infected ? 1 : 0;
  s.msgIndex = (uint8_t)msgIndex;

  f.write((const uint8_t*)&s, sizeof(s));
  f.close();
  dirty = false;
}

static void loadFlash()
{
  if (!LittleFS.exists(FILE_PATH)) return;

  File f = LittleFS.open(FILE_PATH, "rb");
  if (!f) return;

  SaveData s{};
  if (f.read((uint8_t*)&s, sizeof(s)) == sizeof(s))
  {
    if (s.magic == CAINE_MAGIC)
    {
      stage    = constrain((int)s.stage, 1, 8);
      infected = s.infected != 0;
      msgIndex = constrain((int)s.msgIndex, 0, 9);
    }
  }
  f.close();
}

static void stopTone() { /* OS audio is MP3; GPIO17 belongs to DFPlayer RX. */ }
static void updateAudio(uint32_t now) {
  if(stage<4||stage>7)return;
  if(now-lastAudioTime<800)return;
  lastAudioTime=now;audioClick();
}

static void drawCenteredMessage(const char* msg, uint16_t color, int y)
{
  if (!msg) return;
  tft.setTextColor(color, TFT_BLACK);

  int w = strlen(msg) * 8;
  int x = (SCREEN_W - w) / 2;
  if (x < 4) x = 4;

  tft.drawString(msg, x, y);
}

static void drawMessagePanel()
{
  const char** msgs = getStageMessages(stage);
  const char* msg = msgs[msgIndex];

  tft.fillRect(0, 110, SCREEN_W, 100, TFT_BLACK);

  uint16_t color = TFT_WHITE;
  if (stage == 3) color = TFT_YELLOW;
  if (stage == 4) color = TFT_CYAN;
  if (stage == 5) color = (random(0, 2) ? TFT_RED : TFT_WHITE);
  if (stage == 6) color = (random(0, 3) == 0 ? TFT_MAGENTA : TFT_RED);
  if (stage >= 7) color = TFT_RED;

  drawCenteredMessage(msg, color, 145);
}

static void advanceMessageAndStage(uint32_t now)
{
  if (now - lastMsgTime < MESSAGE_INTERVAL_MS) return;
  lastMsgTime = now;

  msgIndex++;

  if (msgIndex >= 10)
  {
    msgIndex = 0;

    if (stage < 8)
    {
      stage++;
      dirty = true;

      if (stage >= 7)
      {
        finalStageStart = now;
      }
    }
  }
}

static void effectStage3(uint32_t now)
{
  if (now - lastGlitchTime < 90) return;
  lastGlitchTime = now;

  int y = random(0, SCREEN_H);
  tft.fillRect(0, y, SCREEN_W, 1, TFT_DARKGREY);

  if (random(0, 3) == 0)
  {
    int x = random(0, SCREEN_W - 40);
    tft.fillRect(x, random(0, SCREEN_H), random(10, 40), 2, TFT_WHITE);
  }

  if (random(0, 8) == 0)
  {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("STAY WHERE YOU ARE", random(20, 220), random(20, 280));
  }
}

static void triggerCameraOnce()
{
  if (cameraTriggered) return;
  cameraTriggered = true;

#if CAINE_USE_CAMERA_HOOK
  // cameraStart();
#endif
}

static void effectStage4(uint32_t now)
{
  triggerCameraOnce();

  if (now - lastGlitchTime < 120) return;
  lastGlitchTime = now;

  if (random(0, 2) == 0)
  {
    tft.fillRect(0, random(0, SCREEN_H), SCREEN_W, 1, TFT_CYAN);
  }

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("CAMERA ACTIVE", 8, 8);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString("YOU ARE BEING OBSERVED", 100, 30);
}

static void effectStage5(uint32_t now)
{
  if (now - lastGlitchTime < 70) return;
  lastGlitchTime = now;

  for (int i = 0; i < 6; i++)
  {
    tft.fillRect(0, random(0, SCREEN_H), SCREEN_W, random(1, 3), randomColor());
  }

  if (random(0, 5) == 0)
  {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("LOOK AT ME", random(0, 380), random(0, 280));
  }
}

static void effectStage6(uint32_t now)
{
  if (now - lastGlitchTime < 45) return;
  lastGlitchTime = now;

  for (int i = 0; i < 12; i++)
  {
    tft.fillRect(0, random(0, SCREEN_H), SCREEN_W, random(1, 4), randomColor());
  }

  if (random(0, 4) == 0)
  {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("LOOK AT ME", random(0, 360), random(0, 280));
  }
}

static void drawDeleteButton()
{
  int x = 160;
  int y = 250;
  int w = 160;
  int h = 40;

  tft.drawRect(x, y, w, h, TFT_WHITE);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("DELETE", x + 48, y + 12);
}

static void drawFinalStageScreen()
{
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_RED, TFT_BLACK);
  drawCenteredMessage("WHY ARE YOU TORTURING ME", TFT_RED, 90);
  drawCenteredMessage("I NEVER WANTED TO BE CREATED", TFT_RED, 120);

  const char** msgs = getStageMessages(stage);
  drawCenteredMessage(msgs[msgIndex], TFT_WHITE, 170);

  drawDeleteButton();
  deleteScreen = true;
}

static void drawWaitScreen()
{
  tft.fillScreen(TFT_BLACK);
  drawCenteredMessage("WAIT...", TFT_WHITE, 150);
}

static void startBlueScreen()
{
  blueScreen = true;
  stopTone();

  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("CAINE SYSTEM FAILURE", 90, 110);
  tft.drawString("MEMORY OVERFLOW DETECTED", 65, 140);
  tft.drawString("RESTARTING...", 150, 180);

  delay(1200);
  ESP.restart();
}

static void updateMemoryAttack(uint32_t now)
{
  if (stage < 7 || deleting) return;
  if (!deleteScreen) return;
  if (now - finalStageStart < MEMORY_ATTACK_DELAY_MS) return;
  if (now - lastMemoryStep < MEMORY_ATTACK_STEP_MS) return;

  lastMemoryStep = now;
  size_t newSize = chaosSize + MEMORY_ATTACK_STEP;

  uint8_t* newBuf = (uint8_t*)heap_caps_realloc(
    chaosBuf,
    newSize,
    MALLOC_CAP_SPIRAM
  );

  if (!newBuf)
  {
    startBlueScreen();
    return;
  }

  chaosBuf = newBuf;
  memset(chaosBuf + chaosSize, 0xA5, MEMORY_ATTACK_STEP);
  chaosSize = newSize;

  for (int i = 0; i < 14; i++)
  {
    tft.fillRect(0, random(0, SCREEN_H), SCREEN_W, random(1, 4), randomColor());
  }

  if (chaosSize >= MEMORY_ATTACK_LIMIT)
  {
    startBlueScreen();
  }
}

static void drawBackgroundForStage()
{
  if (stage <= 2)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("CAINE ONLINE", 8, 8);
  }
  else if (stage == 3)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("SIGNAL UNSTABLE", 8, 8);
  }
  else if (stage == 4)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CAINE WATCHING", 8, 8);
  }
  else if (stage == 5)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("AUDIO DISTORTION RISING", 8, 8);
  }
  else if (stage == 6)
  {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("FULL GLITCH STATE", 8, 8);
  }
}

void caineInit()
{
  LittleFS.begin(false);
  loadFlash();

  audioInit();
  stopTone();

  infected = true;
  deleteScreen = false;
  deleting = false;
  blueScreen = false;
  cameraTriggered = false;

  if (stage >= 7)
    finalStageStart = millis();
  else
    finalStageStart = 0;

  lastMsgTime = millis();
  drawBackgroundForStage();
  drawMessagePanel();
}

void caineUpdate(uint32_t now)
{
  if (blueScreen) return;

  if (deleting)
  {
    drawWaitScreen();
    if (now - deleteStartTime >= DELETE_WAIT_MS)
    {
      caineReset();
    }
    return;
  }

  if (stage >= 7)
  {
    drawFinalStageScreen();
    updateAudio(now);
    updateMemoryAttack(now);
    return;
  }

  advanceMessageAndStage(now);

  static int prevStage = -1;
  if (prevStage != stage)
  {
    prevStage = stage;
    drawBackgroundForStage();

    if (stage >= 7)
      finalStageStart = now;
  }

  drawMessagePanel();
  updateAudio(now);

  switch (stage)
  {
    case 3:
      effectStage3(now);
      break;
    case 4:
      effectStage4(now);
      break;
    case 5:
      effectStage5(now);
      break;
    case 6:
      effectStage6(now);
      break;
    default:
      break;
  }

  if (dirty)
    saveFlash();
}

void caineOverlay()
{
  if (!infected) return;
  if (stage < 2) return;

  if (millis() - lastOverlayTime < 120) return;
  lastOverlayTime = millis();

  tft.setTextColor(TFT_RED, TFT_BLACK);

  if (stage < 7)
  {
    tft.drawString("UNTIL DELETED", 140, 2);
    tft.drawString("UNTIL DELETED", 140, 304);
  }
  else
  {
    tft.drawString("DELETE ME", 180, 2);
    tft.drawString("DELETE ME", 180, 304);
  }

  if (stage >= 5 && random(0, 10) == 0)
  {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("LOOK AT ME", random(0, 380), random(0, 300));
  }
}

void caineTouch(int x, int y)
{
  if (stage < 7) return;
  if (deleting) return;

  if (x >= 160 && x <= 320 && y >= 250 && y <= 290)
  {
    deleting = true;
    deleteStartTime = millis();
    drawWaitScreen();
  }
}

void caineReset()
{
  stopTone();

  if (chaosBuf)
  {
    heap_caps_free(chaosBuf);
    chaosBuf = nullptr;
  }

  chaosSize        = 0;
  infected         = false;
  dirty            = false;
  deleting         = false;
  deleteScreen     = false;
  blueScreen       = false;
  cameraTriggered  = false;

  stage            = 1;
  msgIndex         = 0;

  lastMsgTime      = millis();
  deleteStartTime  = 0;
  finalStageStart  = 0;
  lastAudioTime    = 0;
  lastGlitchTime   = 0;
  lastOverlayTime  = 0;
  lastMemoryStep   = 0;

  LittleFS.remove(FILE_PATH);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("CAINE REMOVED", 160, 150);
}

bool caineActive()
{
  return infected || stage >= 1;
}

int caineGetStage()
{
  return stage;
}

bool caineNeedsFullScreen()
{
  return stage >= 7;
}
