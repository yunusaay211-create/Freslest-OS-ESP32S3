#include "apps.h"
#include "ui_layout.h"
#include "../../gfx.h"
#include "../../input.h"
#include "settings.h"
#include <LittleFS.h>
#include "storage_init.h"
#include "../../usb_input.h"

static String noteText;
static bool noteActive=false;
static int cursor = 0;
static int viewStart = 0;

static const char* CHARSET =
  " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,!?-_/()@:+";

static int charsetLen() {
  return (int)strlen(CHARSET);
}

static void loadNote() {
  noteText = "";
  if (!freslestStorageEnsure()) return;
  if (!LittleFS.exists("/note.txt")) return;
  File f = LittleFS.open("/note.txt", "r");
  if (!f) return;
  noteText = f.readString();
  f.close();
  // keep it reasonable
  if (noteText.length() > 240) noteText.remove(240);
}

static void saveNote() {
  if (!freslestStorageEnsure()) return;
  File f = LittleFS.open("/note.txt", "w");
  if (!f) return;
  f.print(noteText);
  f.close();
}

static void ensureCursor() {
  if (cursor < 0) cursor = 0;
  if (cursor > (int)noteText.length()) cursor = noteText.length();

  // scroll horizontally by characters in the single-line editor area
  const int maxVisible=uiCompact(tft.width())?21:28;
  if (cursor < viewStart) viewStart = cursor;
  if (cursor > viewStart + maxVisible) viewStart = cursor - maxVisible;
  if (viewStart < 0) viewStart = 0;
}

static char getCharAtCursor() {
  if (cursor >= (int)noteText.length()) return ' ';
  return noteText[cursor];
}

static void setCharAtCursor(char c) {
  if (cursor >= (int)noteText.length()) {
    // pad spaces
    while ((int)noteText.length() < cursor) noteText += ' ';
    noteText += c;
  } else {
    noteText.setCharAt(cursor, c);
  }
}

static void drawNote() {
  tft.fillScreen(TH().bg);
  drawHeader("Apps: Note");
  drawFooter("L/R Move  U/D Char", "OK Insert  BACK Save");
  clearBody();

  tft.setTextColor(TH().text, TH().bg);
  tft.setTextSize(1);
  bool compact=uiCompact(tft.width());
  tft.setCursor(compact?12:20,compact?48:60);
  tft.print(compact?"USB keyboard: type / arrows: edit":"USB keyboard: type | physical keys: edit character");

  // editor box
  int boxX=compact?12:18,boxY=compact?74:90,boxW=compact?296:444,boxH=compact?52:60;
  const int columns=compact?22:32;
  tft.fillRoundRect(boxX, boxY, boxW, boxH, 10, TH().panel);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 10, TH().dim);

  ensureCursor();
  String view = noteText.substring(viewStart, std::min((int)noteText.length(), viewStart + columns));
  // pad to show cursor at end
  while (view.length() < (unsigned)columns) view += ' ';

  tft.setTextSize(2);
  tft.setTextColor(TH().text, TH().panel);
  tft.setCursor(boxX + 12, boxY + 20);
  tft.print(view);

  // cursor indicator
  int curPos = cursor - viewStart;
  if (curPos < 0) curPos = 0;
  if(curPos>=columns)curPos=columns-1;
  int cx = boxX + 12 + curPos * 12; // approx for size2
  tft.drawLine(cx, boxY + (compact?42:48), cx + 10, boxY + (compact?42:48), TH().accent);

  // help + info
  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(compact?12:20,compact?148:165);
  tft.print(compact?"OK: insert space / U/D: change char":"Tip: OK inserts space at cursor. U/D changes current char.");
  tft.setCursor(compact?12:20,compact?168:185);
  tft.print("Length: ");
  tft.print(noteText.length());
  tft.print("  Cursor: ");
  tft.print(cursor);
}

void noteEnter() {
  usbInputTextMode(true);noteActive=true;
  loadNote();
  cursor = (int)noteText.length();
  viewStart = 0;
  drawNote();
}

void noteLoop(uint32_t now) {
  (void)now;
  bool redraw = false;
  bool compact=uiCompact(tft.width());
  if(usbInputHit(compact?12:18,compact?74:90,compact?296:444,compact?52:60)){cursor=constrain(viewStart+std::max(0,(usbPointer.x-(compact?24:30))/12),0,(int)noteText.length());redraw=true;}
  int ch;
  while((ch=usbInputCharacter())>=0){
    ensureCursor();
    if(ch==8&&cursor>0){noteText.remove(--cursor,1);redraw=true;}
    else if(ch==127&&cursor<(int)noteText.length()){noteText.remove(cursor,1);redraw=true;}
    else if(ch>=32&&ch<127&&noteText.length()<240){noteText=noteText.substring(0,cursor)+String((char)ch)+noteText.substring(cursor);++cursor;redraw=true;}
  }


  if (bLeft.pressed())  { cursor--; redraw = true; }
  if (bRight.pressed()) { cursor++; redraw = true; }

  if (bUp.pressed() || bDown.pressed()) {
    char cur = getCharAtCursor();
    int idx = 0;
    for (int i = 0; i < charsetLen(); i++) { if (CHARSET[i] == cur) { idx = i; break; } }
    idx += bUp.pressed() ? 1 : -1;
    if (idx < 0) idx = charsetLen() - 1;
    if (idx >= charsetLen()) idx = 0;
    setCharAtCursor(CHARSET[idx]);
    redraw = true;
  }

  if (bOk.pressed()) {
    // insert a space at cursor
    if (noteText.length() < 240) {
      noteText = noteText.substring(0, cursor) + " " + noteText.substring(cursor);
      cursor++;
      redraw = true;
    }
  }

  if (redraw) drawNote();
}

void noteExit(){if(noteActive)saveNote();noteActive=false;usbInputTextMode(false);}
