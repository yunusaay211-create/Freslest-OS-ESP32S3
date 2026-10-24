#include "app_browser.h"
#ifdef FRESLEST_NATIVE_UI_TEST
#include "native_ui_platform.h"
#else
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "apps.h"
#include "browser_native.h"
#include "text_entry.h"
#include <TJpg_Decoder.h>
#endif
#include "browser_native.h"
#include "text_entry.h"
#include <algorithm>
#include <cstring>
namespace {
enum View { Home, Page, Help };
View view = Home;
View beforeHelp = Home;
enum Edit { None, Address, Field };
Edit edit = None;
NativePage *page = nullptr;
int pageSlot = -1, row = 0, top = 0, selected = -1, field = -1;
uint32_t statusRevision = UINT32_MAX;
char address[web_lite::URL] = "https://", history[8][web_lite::URL]{};
unsigned historyCount = 0;
const char *urls[] = {"https://www.google.com/", "https://www.youtube.com/",
                      "https://chatgpt.com/"};
int visibleLines() { return (tft.height() - 116) / 13; }
void release() {
  if (pageSlot >= 0)
    nativeBrowserRelease(pageSlot);
  pageSlot = -1;
  page = nullptr;
}
void footer() { drawFooter("UP/DOWN Scroll L/R Link", "BACK Browser"); }
void toolbar() {
  tft.fillRect(0, 44, 480, 28, TH().panel);
  const char *labels[] = {"< History", "Home", "Address", "Reload", "Info"};
  tft.setTextSize(1);
  for (int i = 0; i < 5; i++) {
    tft.drawRect(20 + i * 88, 47, 84, 21, TH().dim);
    tft.setTextColor(TH().text, TH().panel);
    tft.setCursor(25 + i * 88, 54);
    tft.print(labels[i]);
  }
}
void notice(const char *s) {
  int y = tft.height() - 41;
  tft.fillRect(12, y - 3, 456, 17, TH().bg);
  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(18, y);
  tft.printf("%.73s", s);
}
void drawHome() {
  tft.fillScreen(TH().bg);
  drawHeader("Browser: Standalone");
  clearBody();
  drawFooter("UP/DOWN Select OK Open", "BACK Apps");
  const char *labels[] = {"Google",
                          "YouTube (text only; no video)",
                          "ChatGPT (full web UI unsupported)",
                          "Open address",
                          "Wi-Fi settings",
                          "Browser capabilities"};
  tft.setTextSize(1);
  for (int i = 0; i < 6; i++) {
    int y = 60 + 34 * i;
    tft.fillRoundRect(20, y, 440, 30, 5, i == row ? TH().selbg : TH().panel);
    tft.setTextColor(TH().text, i == row ? TH().selbg : TH().panel);
    tft.setCursor(32, y + 10);
    tft.print(labels[i]);
  }
  notice("Runs on ESP32. HTML/text, links, GET forms, JPEG. No PC required.");
}
void drawHelp() {
  tft.fillScreen(TH().bg);
  drawHeader("Browser: Capabilities");
  clearBody();
  drawFooter("OK / BACK Return", "No external PC");
  const char *lines[] = {
      "The ESP32 downloads pages directly using Wi-Fi.",
      "HTTP and verified HTTPS, HTML text, links, GET forms,",
      "and JPEG images are supported. No server setup is needed.",
      "",
      "CSS layouts and JavaScript are not executed.",
      "YouTube video, MP4/H.264 and the full ChatGPT web app",
      "cannot run in this browser. Their links may show text",
      "or a request to use a full browser.",
      "",
      "UP/DOWN scroll. LEFT/RIGHT selects a link. OK opens it.",
      "Mouse clicks links. Form fields use the screen keyboard.",
      "HTTPS needs valid time: Wi-Fi sets it through NTP."};
  tft.setTextSize(1);
  tft.setTextColor(TH().text, TH().bg);
  for (unsigned i = 0; i < sizeof(lines) / sizeof(*lines); i++) {
    tft.setCursor(18, 60 + i * 17);
    tft.print(lines[i]);
  }
}
bool jpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h,
                uint16_t *pixels) {
  if (x < 18 || y < 76 || x + w > 462 || y + h > tft.height() - 48)
    return false;
  tft.pushImage(x, y, w, h, pixels);
  return true;
}
void drawPage() {
  tft.fillScreen(TH().bg);
  drawHeader(page ? page->document.title : "Browser: Loading");
  clearBody();
  toolbar();
  footer();
  tft.setTextSize(1);
  if (!page) {
    notice(nativeBrowserStatus().message);
    return;
  }
  const char *imageError = nullptr;
  if (page->image && page->jpegLength) {
    uint16_t w = 0, h = 0;
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(jpegOutput);
    if (TJpgDec.getJpgSize(&w, &h, page->jpeg, page->jpegLength) == JDR_OK &&
        w && h) {
      unsigned scale = 1;
      while (scale < 8 &&
             ((w + scale - 1) / scale > 444 ||
              (h + scale - 1) / scale > (unsigned)tft.height() - 124))
        scale *= 2;
      if ((w + scale - 1) / scale <= 444 &&
          (h + scale - 1) / scale <= (unsigned)tft.height() - 124) {
        TJpgDec.setJpgScale(scale);
        TJpgDec.drawJpg(18 + (444 - (w + scale - 1) / scale) / 2, 76,
                        page->jpeg, page->jpegLength);
        TJpgDec.setJpgScale(1);
      } else
        imageError = "JPEG dimensions are too large for the display.";
    } else
      imageError = "Unsupported or invalid JPEG.";
  } else {
    int count = visibleLines();
    for (int i = 0; i < count && top + i < page->document.lineCount; i++) {
      auto &line = page->document.lines[top + i];
      int y = 77 + i * 13;
      bool focused = line.link >= 0 && line.link == selected;
      if (focused)
        tft.fillRect(16, y - 2, 448, 13, TH().selbg);
      tft.setTextColor(line.link >= 0 ? TH().accent : TH().text,
                       focused ? TH().selbg : TH().bg);
      tft.setCursor(18, y);
      if (line.link >= 0 &&
          page->document.links[line.link].kind == web_lite::Field) {
        auto &f = page->document.fields[page->document.links[line.link].index];
        tft.printf("[%.30s] %.35s", f.name, f.secret ? "***" : f.value);
      } else
        tft.print(line.text);
    }
    if (page->document.lineCount == 1 && !page->document.lines[0].text[0]) {
      tft.setCursor(18, 98);
      tft.printf("%.72s", page->message);
      if (strlen(page->message) > 72) {
        tft.setCursor(18, 116);
        tft.printf("%.47s", page->message + 72);
      }
    }
  }
  notice(imageError ? imageError : page->message);
}
void home() {
  nativeBrowserCancel();
  release();
  view = Home;
  row = 0;
  usbInputTextMode(false);
  drawHome();
}
void open(const char *url, bool remember = true) {
  if (!web_lite::validUrl(url)) {
    notice("Enter a complete http:// or https:// address.");
    return;
  }
  if (!nativeBrowserOpen(url)) {
    notice("Cannot start browser. Check PSRAM and try again.");
    return;
  }
  if (remember) {
    if (historyCount == 8) {
      memmove(history, history + 1, 7 * sizeof(history[0]));
      --historyCount;
    }
    strlcpy(history[historyCount++], url, sizeof(history[0]));
  }
  strlcpy(address, url, sizeof(address));
  release();
  view = Page;
  top = 0;
  selected = -1;
  drawPage();
  statusRevision = UINT32_MAX;
}
void back() {
  if (historyCount > 1) {
    --historyCount;
    open(history[historyCount - 1], false);
  } else
    home();
}
void editAddress() {
  edit = Address;
  textEntryBegin("Browser: Address", address, 383);
}
void follow(int link) {
  if (!page || link < 0 || link >= page->document.linkCount)
    return;
  auto item = page->document.links[link];
  if (item.kind == web_lite::Field) {
    field = item.index;
    auto &f = page->document.fields[field];
    edit = Field;
    textEntryBegin(f.name, f.value, sizeof(f.value) - 1, f.secret);
    return;
  }
  if (item.kind == web_lite::Submit) {
    char url[web_lite::URL];
    if (web_lite::formUrl(page->document, item.index, url))
      open(url);
    else
      notice(
          "This form requires POST or exceeds the address limit; unsupported.");
    return;
  }
  if (item.kind == web_lite::Video) {
    notice("Video codec unavailable. This device browser cannot play "
           "YouTube/MP4.");
    return;
  }
  open(item.url);
}
void moveLink(int direction) {
  if (!page || !page->document.linkCount)
    return;
  int count = page->document.linkCount;
  selected = selected < 0 ? (direction > 0 ? 0 : count - 1)
                          : (selected + count + direction) % count;
  for (int i = 0; i < page->document.lineCount; i++)
    if (page->document.lines[i].link == selected) {
      if (i < top || i >= top + visibleLines())
        top = std::min(
            i, std::max(0, (int)page->document.lineCount - visibleLines()));
      break;
    }
  drawPage();
}
} // namespace
void browserEnter() {
  edit = None;
  usbInputTextMode(false);
  if (view == Page && page)
    drawPage();
  else {
    view = Home;
    row = 0;
    drawHome();
  }
}
void browserExit() {
  nativeBrowserCancel();
  usbInputTextMode(false);
  textEntryCancel();
  edit = None;
}
void browserLoop(uint32_t) {
  char text[384];
  auto result = textEntryTakeResult(text, sizeof(text));
  if (edit != None && result != TextEntryResult::None) {
    Edit last = edit;
    edit = None;
    if (result == TextEntryResult::Accepted) {
      if (last == Address) {
        open(text);
        return;
      }
      if (last == Field && page && field >= 0 &&
          field < page->document.fieldCount)
        strlcpy(page->document.fields[field].value, text,
                sizeof(page->document.fields[field].value));
    }
    memset(text, 0, sizeof(text));
    if (view == Page)
      drawPage();
    else
      drawHome();
    return;
  }
  if (view == Help) {
    if (bBack.pressed() || bOk.pressed() || usbPointer.click) {
      view = beforeHelp;
      if (view == Page)
        drawPage();
      else
        drawHome();
    }
    return;
  }
  if (bBack.pressed()) {
    if (view == Page)
      home();
    else
      enterApp(APP_APPS);
    return;
  }
  if (view == Home) {
    bool changed = false;
    if (bUp.pressed()) {
      row = (row + 5) % 6;
      changed = true;
    }
    if (bDown.pressed()) {
      row = (row + 1) % 6;
      changed = true;
    }
    bool choose = bOk.pressed();
    if (usbInputHit(20, 60, 440, 204)) {
      row = (usbPointer.y - 60) / 34;
      choose = true;
    }
    if (changed)
      drawHome();
    if (choose) {
      if (row < 3)
        open(urls[row]);
      else if (row == 3)
        editAddress();
      else if (row == 4)
        enterApp(APP_SET_WIFI);
      else {
        beforeHelp = Home;
        view = Help;
        drawHelp();
      }
    }
    return;
  }
  int incoming = -1;
  if (auto *next = nativeBrowserTake(incoming)) {
    release();
    page = next;
    pageSlot = incoming;
    top = 0;
    selected = -1;
    strlcpy(address, page->document.url, sizeof(address));
    drawPage();
  }
  auto status = nativeBrowserStatus();
  if (status.revision != statusRevision) {
    statusRevision = status.revision;
    if (status.busy)
      notice(status.message);
  }
  for (int i = 0; i < 5; i++)
    if (usbInputHit(20 + i * 88, 47, 84, 21)) {
      if (i == 0)
        back();
      if (i == 1)
        home();
      if (i == 2)
        editAddress();
      if (i == 3)
        open(address, false);
      if (i == 4) {
        beforeHelp = Page;
        view = Help;
        drawHelp();
      }
      return;
    }
  if (bFire.pressed()) {
    editAddress();
    return;
  }
  if (!page)
    return;
  bool redraw = false;
  int limit = std::max(0, (int)page->document.lineCount - visibleLines());
  if (bUp.pressed()) {
    top = std::max(0, top - 1);
    redraw = true;
  }
  if (bDown.pressed()) {
    top = std::min(limit, top + 1);
    redraw = true;
  }
  if (bLeft.pressed())
    moveLink(-1);
  if (bRight.pressed())
    moveLink(1);
  if (usbInputHit(16, 75, 448, visibleLines() * 13)) {
    int line = top + (usbPointer.y - 75) / 13;
    if (line < page->document.lineCount) {
      selected = page->document.lines[line].link;
      follow(selected);
      return;
    }
  }
  if (bOk.pressed())
    follow(selected);
  if (redraw)
    drawPage();
}
