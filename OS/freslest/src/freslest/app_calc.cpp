#include "apps.h"
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

static void drawCalc() {
  tft.fillScreen(TH().bg);
  drawHeader("Apps: Calculator");
  drawFooter("L/R Field  U/D Value", "OK Action  BACK Menu");
  clearBody();

  tft.setTextColor(TH().text, TH().bg);
  tft.setTextSize(3);

  int y = 95;
  auto drawBox = [&](int x, int w, const char* label, String val, bool sel) {
    uint16_t fill = sel ? TH().selbg : TH().panel;
    tft.fillRoundRect(x, y, w, 58, 10, fill);
    tft.drawRoundRect(x, y, w, 58, 10, sel ? TH().accent : TH().dim);
    tft.setTextColor(TH().text, fill);
    tft.setTextSize(1);
    tft.setCursor(x + 10, y + 8);
    tft.print(label);
    tft.setTextSize(2);
    tft.setCursor(x + 10, y + 28);
    tft.print(val);
  };

  drawBox(20, 150, "A", String(A), field==0);
  drawBox(180, 60, "OP", opStr(op), field==1);
  drawBox(250, 150, "B", String(B), field==2);
  drawBox(410, 50, "=", "", field==3);

  tft.setTextSize(2);
  tft.setTextColor(TH().accent, TH().bg);
  tft.setCursor(20, 190);
  tft.print("Result: ");
  tft.setTextColor(TH().text, TH().bg);
  tft.print(R);

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(20, 225);
  tft.print("OK on '=' computes. OK on OP cycles. Division by 0 -> 0.");
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
  const int xs[]={20,180,250,410},ws[]={150,60,150,50};
  for(int i=0;i<4;i++)if(usbInputHit(xs[i],95,ws[i],58)){field=i;redraw=true;if(i==1||i==3)bOk.pressedEdge=true;}


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
