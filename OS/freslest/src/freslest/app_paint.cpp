#include "apps.h"
#include "ui_layout.h"
#include "../../gfx.h"
#include "../../input.h"
#include "settings.h"
#include "../../usb_input.h"

static constexpr int CW = 40;   // columns
static constexpr int CH = 24;   // rows (fit screen)

static int paintCell(){return uiCompact(tft.width())?6:9;}
static int paintX(){return (tft.width()-CW*paintCell())/2;}
static int paintY(){return uiCompact(tft.width())?54:60;}

static uint8_t canvas[CH][CW];
static int cx = CW/2;
static int cy = CH/2;

static uint16_t penColor = 0xFFFF; // white
static uint16_t bgColor = 0x0000;  // black-ish body background uses theme

static uint32_t okDownAt = 0;
static bool okWasDown = false;

static void drawCell(int x, int y) {
  uint16_t c = canvas[y][x] ? penColor : TH().bg;
  tft.fillRect(paintX() + x*paintCell(), paintY() + y*paintCell(), paintCell(), paintCell(), c);
}

static void drawCursor(bool on) {
  int x = paintX() + cx*paintCell();
  int y = paintY() + cy*paintCell();
  uint16_t c = on ? TH().accent : TH().dim;
  tft.drawRect(x, y, paintCell(), paintCell(), c);
}

static void drawPaintFull() {
  // no full screen clear (avoids flash)
  drawHeader("Apps: Paint");
  drawFooter("D-Pad Move", "OK Draw  Hold OK Color  BACK Menu");
  clearBody();

  // canvas area
  tft.fillRoundRect(paintX()-4, paintY()-4, CW*paintCell()+8, CH*paintCell()+8, 10, TH().panel);
  tft.drawRoundRect(paintX()-4, paintY()-4, CW*paintCell()+8, CH*paintCell()+8, 10, TH().dim);

  for (int y=0;y<CH;y++){
    for(int x=0;x<CW;x++){
      if (canvas[y][x]) drawCell(x,y);
    }
  }
  drawCursor(true);

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(uiCompact(tft.width())?12:20,uiCompact(tft.width())?40:48);
  tft.print(uiCompact(tft.width())?"Hold OK: change pen color":"Hold OK: cycle pen color (white/cyan/yellow/pink/green)");
}

static void cycleColor() {
  // simple 5-color cycle
  if (penColor == 0xFFFF) penColor = 0x07FF;      // cyan
  else if (penColor == 0x07FF) penColor = 0xFFE0; // yellow
  else if (penColor == 0xFFE0) penColor = 0xF81F; // pink
  else if (penColor == 0xF81F) penColor = 0x07E0; // green
  else penColor = 0xFFFF;
}

void paintEnter() {
  // keep canvas content in RAM (no auto-clear)
  drawPaintFull();
}

void paintLoop(uint32_t now) {
  bool moved=false;
  if(usbPointer.visible&&usbPointer.left&&usbPointer.x>=paintX()&&usbPointer.x<paintX()+CW*paintCell()&&usbPointer.y>=paintY()&&usbPointer.y<paintY()+CH*paintCell()){drawCell(cx,cy);cx=(usbPointer.x-paintX())/paintCell();cy=(usbPointer.y-paintY())/paintCell();canvas[cy][cx]=1;drawCell(cx,cy);drawCursor(true);}


  if (bLeft.pressed())  { drawCursor(false); cx = (cx-1+CW)%CW; moved=true; }
  if (bRight.pressed()) { drawCursor(false); cx = (cx+1)%CW; moved=true; }
  if (bUp.pressed())    { drawCursor(false); cy = (cy-1+CH)%CH; moved=true; }
  if (bDown.pressed())  { drawCursor(false); cy = (cy+1)%CH; moved=true; }

  if (moved) {
    drawCursor(true);
  }

  // detect OK hold for color
  bool okDown = bOk.down();
  if (okDown && !okWasDown) { okDownAt = now; okWasDown = true; }
  if (!okDown && okWasDown) {
    uint32_t held = now - okDownAt;
    okWasDown = false;
    if (held >= 500) {
      cycleColor();
      // small color indicator
      tft.fillRect(tft.width()-84,uiCompact(tft.width())?40:48,72,14,TH().bg);
      tft.setTextColor(penColor, TH().bg);
      tft.setCursor(tft.width()-84,uiCompact(tft.width())?40:48);
      tft.setTextSize(1);
      tft.print("PEN");
      return;
    } else {
      // short press -> toggle pixel
      canvas[cy][cx] ^= 1;
      drawCell(cx, cy);
      drawCursor(true);
    }
  }
}
