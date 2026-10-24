#include "text_entry.h"
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "apps.h"
#include <algorithm>
namespace {
TextEntryState edit;
bool active = false;
TextEntryResult result = TextEntryResult::None;
void draw() {
  clearBody();
  tft.setTextSize(1);
  tft.setTextColor(TH().text, TH().panel);
  tft.fillRoundRect(18, 57, 444, 42, 5, TH().panel);
  unsigned start = edit.cursor > 67 ? edit.cursor - 67 : 0;
  char view[71]{};
  unsigned n = std::min(70u, edit.length - start);
  for (unsigned i = 0; i < n; i++)
    view[i] = edit.secret ? '*' : edit.text[start + i];
  tft.setCursor(25, 69);
  tft.print(view);
  int cx = 25 + 6 * (int)(edit.cursor - start);
  tft.drawLine(cx, 84, cx + 4, 84, TH().accent);
  for (unsigned i = 0; i < 40; i++) {
    int x = 20 + (i % 10) * 44, y = 108 + (i / 10) * 30;
    bool sel = edit.focus == i;
    tft.fillRoundRect(x, y, 41, 27, 4, sel ? TH().selbg : TH().panel);
    if (sel)
      tft.drawRoundRect(x, y, 41, 27, 4, TH().accent);
    TextEntryState item = edit;
    item.focus = i;
    char c[2] = {item.focusedChar(), 0};
    tft.setTextColor(TH().text, sel ? TH().selbg : TH().panel);
    tft.setCursor(x + 17, y + 10);
    tft.print(c);
  }
  const char *labels[] = {"Space", "Delete", "Case", "Done", "Cancel"};
  for (unsigned i = 0; i < 5; i++) {
    int x = 20 + i * 88;
    bool sel = edit.focus == 40 + i;
    tft.fillRoundRect(x, 239, 85, 31, 5, sel ? TH().selbg : TH().panel);
    if (sel)
      tft.drawRoundRect(x, 239, 85, 31, 5, TH().accent);
    tft.setTextColor(TH().text, sel ? TH().selbg : TH().panel);
    tft.setCursor(x + 11, 251);
    tft.print(labels[i]);
  }
}
void finish(TextEntryResult r) {
  active = false;
  result = r;
  usbInputTextMode(false);
}
void choose() {
  if (edit.focus < 40)
    edit.insert(edit.focusedChar());
  else if (edit.focus == 40)
    edit.insert(' ');
  else if (edit.focus == 41)
    edit.erase(true);
  else if (edit.focus == 42)
    edit.page = (edit.page + 1) % 3;
  else
    finish(edit.focus == 43 ? TextEntryResult::Accepted
                            : TextEntryResult::Cancelled);
}
} // namespace
void textEntryBegin(const char *title, const char *value, unsigned limit,
                    bool secret) {
  edit.begin(value, limit, secret);
  active = true;
  result = TextEntryResult::None;
  usbInputTextMode(true);
  tft.fillScreen(TH().bg);
  drawHeader(title);
  drawFooter("Arrows Select  OK Type", "FIRE Case BACK Cancel");
  draw();
}
bool textEntryActive() { return active; }
void textEntryLoop() {
  if (!active)
    return;
  bool redraw = false;
  int c;
  while ((c = usbInputCharacter()) >= 0) {
    if (c == 8)
      redraw |= edit.erase(true);
    else if (c == 127)
      redraw |= edit.erase(false);
    else
      redraw |= edit.insert((char)c);
  }
  if (bBack.pressed()) {
    finish(TextEntryResult::Cancelled);
    return;
  }
  if (bLeft.pressed()) {
    if (usbInputHeld() & 4)
      edit.moveCursor(-1);
    else
      edit.moveFocus(-1, 0);
    redraw = true;
  }
  if (bRight.pressed()) {
    if (usbInputHeld() & 8)
      edit.moveCursor(1);
    else
      edit.moveFocus(1, 0);
    redraw = true;
  }
  if (bUp.pressed()) {
    edit.moveFocus(0, -1);
    redraw = true;
  }
  if (bDown.pressed()) {
    edit.moveFocus(0, 1);
    redraw = true;
  }
  if (bFire.pressed()) {
    edit.page = (edit.page + 1) % 3;
    redraw = true;
  }
  bool click = false;
  for (unsigned i = 0; i < 40; i++)
    if (usbInputHit(20 + (i % 10) * 44, 108 + (i / 10) * 30, 41, 27)) {
      edit.focus = i;
      click = true;
      break;
    }
  for (unsigned i = 0; i < 5; i++)
    if (usbInputHit(20 + i * 88, 239, 85, 31)) {
      edit.focus = 40 + i;
      click = true;
      break;
    }
  if (bOk.pressed() || click) {
    if (!click && (usbInputHeld() & 16))
      finish(TextEntryResult::Accepted);
    else
      choose();
    redraw = true;
  }
  if (active && redraw)
    draw();
}
TextEntryResult textEntryTakeResult(char *out, size_t size) {
  TextEntryResult r = result;
  if (r == TextEntryResult::None)
    return r;
  if (out && size) {
    size_t n = std::min(size - 1, (size_t)edit.length);
    memcpy(out, edit.text, n);
    out[n] = 0;
  }
  memset(edit.text, 0, sizeof(edit.text));
  edit.length = edit.cursor = 0;
  result = TextEntryResult::None;
  return r;
}
void textEntryCancel() {
  if (active)
    finish(TextEntryResult::Cancelled);
}
