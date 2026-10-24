#include "gfx.h"
#include "settings.h"

 

enum BootState {
  BOOT_OK,
  BOOT_WARNING,
  BOOT_FATAL
};

inline BootState checkSystem() {
  bool coreOK = true;
  bool optionalOK = true;

  // Core checks
  if (!psramFound()) coreOK = false;
  if (brightnessUI <= 0) coreOK = false;

  // Optional checks
  // Buraya istersen sonra modül kontrolleri eklersin

  if (!coreOK) return BOOT_FATAL;
  if (!optionalOK) return BOOT_WARNING;
  return BOOT_OK;
}

inline void drawBar(int progress) {
  int x = 100;
  int y = 230;
  int w = 240;
  int h = 16;

  tft.drawRect(x, y, w, h, TFT_WHITE);
  int fill = (progress * (w - 4)) / 100;
  tft.fillRect(x + 2, y + 2, fill, h - 4, TFT_CYAN);
}



inline void drawWarning() {



  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  tft.setTextSize(2);
  tft.setCursor(60, 100);
  tft.print("WARNING");

  tft.setTextSize(1);
  tft.setCursor(40, 140);
  tft.print("Some features are disabled");

  tft.setCursor(60, 200);
  tft.print("[ PRESS OK ]");
}

inline void drawFatal() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);

  tft.setTextSize(2);
  tft.setCursor(60, 100);
  tft.print("SYSTEM ERROR");

  tft.setTextSize(1);
  tft.setCursor(40, 140);
  tft.print("Freslest cannot boot");
}

inline BootState showBootScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setTextSize(3);
  tft.setCursor(120, 120);
  tft.print("FRESLEST OS");

  tft.setTextSize(1);
  tft.setCursor(120, 170);
  tft.print("System checking...");

  

  for (int i = 0; i <= 100; i += 10) {
    drawBar(i);
    delay(60);
  }

  delay(200);
  return checkSystem();
}