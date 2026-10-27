#include "apps.h"
#include "ui_layout.h"
#include "../../gfx.h"
#include "../../input.h"
#include "settings.h"
#include "../../usb_input.h"

static int32_t A = 0;
static int32_t B = 0;
static int32_t R = 0;
static int op = 0; // 0:+ 1:- 2:* 3:/
static int field = 0; // 0 A, 1 op, 2 B, 3 =
static bool dirty = true;

static const char* opStr(int o) {
  switch (o) {
    case 0: return "+";
    case 1: return "-";
    case 2: return "*";
    case 3: return "/";
  }
  return "?";
}

static void compute() {
  switch (op) {
    case 0: R = A + B; break;
    case 1: R = A - B; break;
    case 2: R = A * B; break;
    case 3: R = (B == 0) ? 0 : (A / B); break;
  }
}

static int calcFieldX(int i){const int full[]={20,180,250,410},small[]={12,112,152,252};return uiCompact(tft.width())?small[i]:full[i];}
static int calcFieldW(int i){const int full[]={150,60,150,50},small[]={96,36,96,56};return uiCompact(tft.width())?small[i]:full[i];}
static int calcBoxY(){return uiCompact(tft.width())?60:95;}
static int calcBoxH(){return uiCompact(tft.width())?54:58;}

static void drawCalc() {
  tft.fillScreen(TH().bg);
  drawHeader("Apps: Calculator");
  drawFooter("L/R Field  U/D Value", "OK Action  BACK Menu");
  clearBody();

  tft.setTextColor(TH().text, TH().bg);
  tft.setTextSize(3);

  bool compact=uiCompact(tft.width());
  int y=calcBoxY(),h=calcBoxH();
  auto drawBox = [&](int x, int w, const char* label, String val, bool sel) {
    uint16_t fill = sel ? TH().selbg : TH().panel;
    tft.fillRoundRect(x, y, w, h, 10, fill);
    tft.drawRoundRect(x, y, w, h, 10, sel ? TH().accent : TH().dim);
    tft.setTextColor(TH().text, fill);
    if(compact&&!strcmp(label,"=")){
      tft.setTextSize(2);tft.setCursor(x+(w-12)/2,y+20);tft.print("=");return;
    }
    tft.setTextSize(1);
    tft.setCursor(x + 10, y + 8);
    tft.print(label);
    tft.setTextSize(compact&&val.length()>(unsigned)((w-20)/12)?1:2);
    tft.setCursor(x + 10, y + 28);
    tft.print(val);
  };

  drawBox(calcFieldX(0), calcFieldW(0), "A", String(A), field==0);
  drawBox(calcFieldX(1), calcFieldW(1), "OP", opStr(op), field==1);
  drawBox(calcFieldX(2), calcFieldW(2), "B", String(B), field==2);
  drawBox(calcFieldX(3), calcFieldW(3), "=", "", field==3);

  tft.setTextSize(2);
  tft.setTextColor(TH().accent, TH().bg);
  tft.setCursor(compact?12:20,compact?144:190);
  tft.print("Result: ");
  tft.setTextColor(TH().text, TH().bg);
  tft.print(R);

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(compact?12:20,compact?185:225);
  tft.print(compact?"OK =: calculate / OK OP: change":"OK on '=' computes. OK on OP cycles. Division by 0 -> 0.");
  if(compact){tft.setCursor(12,201);tft.print("Division by 0 -> 0");}
}

void calcEnter() {
  dirty = true;
  compute();
  drawCalc();
  dirty = false;
}

void calcLoop(uint32_t now) {
  (void)now;
  bool redraw = false;
  for(int i=0;i<4;i++)if(usbInputHit(calcFieldX(i),calcBoxY(),calcFieldW(i),calcBoxH())){field=i;redraw=true;if(i==1||i==3)bOk.pressedEdge=true;}


  if (bLeft.pressed())  { field = (field + 3) % 4; redraw = true; }
  if (bRight.pressed()) { field = (field + 1) % 4; redraw = true; }

  if (field == 0 || field == 2) {
    int32_t& v = (field == 0) ? A : B;
    if (bUp.pressed())   { v += 1; redraw = true; }
    if (bDown.pressed()) { v -= 1; redraw = true; }
  }

  if (bOk.pressed()) {
    if (field == 1) { op = (op + 1) % 4; redraw = true; }
    if (field == 3) { compute(); redraw = true; }
  }

  if (redraw) {
    compute();
    drawCalc();
  }
}
