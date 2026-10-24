#ifdef FRESLEST_FILES_HOST_TEST
#include "files_host_platform.h"
#else
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "apps.h"
#include "storage_init.h"
#include <LittleFS.h>
#endif
#include "file_browser.h"
#include "usb_storage.h"
#include <algorithm>
#include <cstring>
#ifndef FRESLEST_INTERNAL_ROOT
#define FRESLEST_INTERNAL_ROOT "/littlefs"
#endif
namespace {
FileBrowser browser;
enum View { DISKS, LIST, ACTIONS, DETAILS, TEXT, DELETE_CONFIRM, ERROR_VIEW };
View view = DISKS;
unsigned selected = 0, top = 0, action = 0, page = 0;
bool confirmDelete = false, more = false, ready = false;
bool onUsb=false;
unsigned disk=0;
uint32_t diskRevision=0;
size_t used = 0, total = 0;
char preview[FileBrowser::TEXT_PAGE + 1], message[96];
constexpr unsigned ROWS = 6;
struct UsbAccess {
  bool held=false,ok=true;
  explicit UsbAccess(bool needed){if(needed)ok=held=usbStorageLock();}
  ~UsbAccess(){if(held)usbStorageUnlock();}
};
void refresh() {
  ready = (onUsb?usbStorageInfo().state==UsbStorageState::Ready:freslestStorageEnsure()) && browser.refresh();
  if (ready&&!onUsb) {
    total = LittleFS.totalBytes();
    used = LittleFS.usedBytes();
  }
  if (!browser.count())
    selected = 0;
  else if (selected >= browser.count())
    selected = browser.count() - 1;
  top = selected / ROWS * ROWS;
}
void label(int x, int y, const char *text, unsigned size = 1) {
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(text);
}
void button(int x, int y, int w, const char *text, bool active) {
  uint16_t bg = active ? TH().selbg : TH().panel;
  tft.fillRoundRect(x, y, w, 28, 6, bg);
  tft.drawRoundRect(x, y, w, 28, 6, active ? TH().accent : TH().dim);
  tft.setTextColor(TH().text, bg);
  label(x + 10, y + 9, text);
}
void draw() {
  tft.fillScreen(TH().bg);
  drawHeader("Apps: Files");
  clearBody();
  drawFooter(view == DISKS ? "OK Open  FIRE Eject USB" : view == LIST ? "OK Open  FIRE Actions" : "OK Select",
             view == DISKS ? "BACK Apps" : view == LIST ? "BACK Parent / Disks" : "BACK Cancel");
  tft.setTextColor(TH().dim, TH().bg);
  char text[80];
  if(view==DISKS){
    UsbStorageInfo usb=usbStorageInfo();
    label(20,56,"Disks",2);
    button(20,96,440,"C:  Internal flash",disk==0);
    tft.setTextColor(TH().dim,TH().bg);
    label(30,134,freslestStorageStatus());
    if(usb.present()){
      button(20,164,440,"D:  USB drive",disk==1);
      tft.setTextColor(TH().dim,TH().bg);
      if(usb.state==UsbStorageState::Ready){
        if(usb.bytes)snprintf(text,sizeof(text),"FAT16/FAT32 | Drive capacity: %llu MB",(unsigned long long)(usb.bytes/(1024*1024)));
        else snprintf(text,sizeof(text),"FAT USB drive ready");
        label(30,202,text);
      }
    }else label(24,180,"Connect a USB drive to USB/OTG.");
    snprintf(text,sizeof(text),"%.72s",usb.message);label(24,224,text);
    if(!usbInputHostMode())button(20,250,300,"FIRE: Switch to USB host",false);
    return;
  }
  if(onUsb)snprintf(text,sizeof(text),"D: USB drive | FAT16/FAT32");
  else snprintf(text, sizeof(text), "C: Internal flash | %u / %u KB used",
           (unsigned)(used / 1024), (unsigned)(total / 1024));
  label(16, 48, text);
  snprintf(text, sizeof(text), "%c:%.70s",onUsb?'D':'C',browser.path());
  label(16, 64, text);
  if (!ready) {
    label(20, 112, "Storage unavailable", 2);
    label(20, 152, onUsb?usbStorageInfo().message:freslestStorageStatus());
    label(20, 182, browser.error());
    button(20, 230, 160, "OK: Retry", true);
    return;
  }
  if (view == LIST) {
    if (!browser.count()) {
      label(30, 120, "This folder is empty", 2);
      label(30, 156, "BACK opens the parent folder.");
    }
    for (unsigned row = 0; row < ROWS && top + row < browser.count(); row++) {
      unsigned i = top + row;
      const auto *e = browser.entry(i);
      bool sel = i == selected;
      int y = 84 + row * 30;
      uint16_t bg = sel ? TH().selbg : TH().bg;
      tft.fillRoundRect(16, y, 448, 28, 6, bg);
      if (sel)
        tft.drawRoundRect(16, y, 448, 28, 6, TH().accent);
      tft.setTextColor(sel ? TH().text : TH().dim, bg);
      snprintf(text, sizeof(text), "%s %.31s", e->directory ? "[+]" : "   ",
               e->name);
      label(24, y + 6, text, 2);
    }
    tft.setTextColor(TH().dim, TH().bg);
    snprintf(text, sizeof(text), "%u items | L/R Page%s", browser.count(),
             browser.truncated() ? " | List limited" : "");
    label(20, 278, text);
    button(358, 268, 106, "Actions", false);
    return;
  }
  const auto *e = browser.entry(selected);
  if (!e) {
    view = LIST;
    draw();
    return;
  }
  snprintf(text, sizeof(text), "%.36s", e->name);
  tft.setTextColor(TH().text, TH().bg);
  label(20, 86, text, 2);
  if (view == ACTIONS) {
    const char *actions[] = {"Open", "Details", "Delete"};
    for (unsigned i = 0; i < 3; i++)
      button(40, 126 + i * 42, 400, actions[i], action == i);
  } else if (view == DETAILS) {
    label(24, 124,
          e->directory               ? "Type: folder"
          : browser.isText(selected) ? "Type: text file"
                                     : "Type: binary file");
    snprintf(text, sizeof(text), "Size: %lu bytes", (unsigned long)e->bytes);
    label(24, 148, text);
    label(24, 178, onUsb?"Location: D: USB FAT":"Location: C: internal LittleFS");
    label(24, 202, onUsb?"USB drive files are separate from internal storage.":"Note: C:/note.txt | NES: C:/games/nes");
    label(24, 232, "DFPlayer microSD is managed separately.");
  } else if (view == TEXT) {
    // Fixed byte pages show every byte, even when a file has many short lines.
    size_t n = strlen(preview);
    for (unsigned row = 0; row < 12 && row * 64 < n; row++) {
      char line[65] = {};
      size_t offset = row * 64, length = std::min<size_t>(64, n - offset);
      memcpy(line, preview + offset, length);
      for (size_t i = 0; i < length; i++)
        if (line[i] == '\n' || line[i] == '\r' || line[i] == '\t')
          line[i] = ' ';
      label(20, 112 + row * 13, line);
    }
    snprintf(text, sizeof(text), "Page %u | L/R Page%s", page + 1,
             more ? " | More" : " | End");
    label(20, 278, text);
  } else if (view == DELETE_CONFIRM) {
    label(24, 132,
          e->directory ? "Delete this empty folder?"
                       : "Permanently delete this file?",
          2);
    label(24, 168, "This cannot be undone. BACK cancels.");
    button(40, 218, 180, "Cancel", !confirmDelete);
    button(260, 218, 180, "Delete", confirmDelete);
  } else {
    tft.setTextColor(TFT_RED, TH().bg);
    label(20, 140, message);
    label(20, 186, "OK / BACK: return to files");
  }
}
void error() {
  snprintf(message, sizeof(message), "%.95s", browser.error());
  view = ERROR_VIEW;
  draw();
}
void open() {
  const auto *e = browser.entry(selected);
  if (!e)
    return;
  if (e->directory) {
    if (!browser.openDirectory(selected)) {
      error();
      return;
    }
    selected = top = 0;
    view = LIST;
    draw();
    return;
  }
  if (!browser.isText(selected)) {
    view = DETAILS;
    draw();
    return;
  }
  page = 0;
  if (!browser.textPage(selected, page, preview, sizeof(preview), more)) {
    error();
    return;
  }
  view = TEXT;
  draw();
}
} // namespace
void filesEnter() {
  usbStorageBegin(); // Allocate the MSC service only when Files is opened.
  view = DISKS;onUsb=false;disk=0;
  selected = top = 0;ready=freslestStorageEnsure();
  diskRevision=usbStorageInfo().revision;
  draw();
}
void filesLoop(uint32_t now) {
  (void)now;
  UsbStorageInfo usb=usbStorageInfo();
  if(usb.revision!=diskRevision){
    diskRevision=usb.revision;
    if(onUsb&&view!=DISKS){view=DISKS;browser.selectMount(usbStorageRoot());selected=top=0;}
    if(!usb.present()&&disk==1)disk=0;
    if(view==DISKS)draw();
  }
  if(view==DISKS){
    if(bBack.pressed()||usbPointer.rightClick){enterApp(APP_APPS);return;}
    if(!usbInputHostMode()&&(bFire.pressed()||usbInputHit(20,250,300,28))){usbInputSwitchMode(false);return;}
    bool ok=bOk.pressed(),changed=false;
    if((bUp.pressed()||bDown.pressed()||bLeft.pressed()||bRight.pressed())&&usb.present()){disk^=1;changed=true;}
    if(usbInputHit(20,96,440,28)){disk=0;ok=true;}
    if(usb.present()&&usbInputHit(20,164,440,28)){disk=1;ok=true;}
    if(bFire.pressed()&&usb.present()){usbStorageEject();return;}
    if(changed)draw();
    if(!ok)return;
    if(disk==1&&usb.state!=UsbStorageState::Ready){
      if(usb.state==UsbStorageState::Error||usb.state==UsbStorageState::Ejected)usbStorageRetry();
      return;
    }
    onUsb=disk==1;UsbAccess access(onUsb);if(!access.ok)return;
    browser.selectMount(onUsb?usbStorageRoot():FRESLEST_INTERNAL_ROOT);
    selected=top=0;view=LIST;refresh();draw();return;
  }
  UsbAccess access(onUsb);
  if(!access.ok){view=DISKS;draw();return;}
  if (usbPointer.rightClick) {
    if (view == LIST && usbPointer.x >= 16 && usbPointer.x < 464 &&
        usbPointer.y >= 84 && usbPointer.y < 264) {
      unsigned i = top + (usbPointer.y - 84) / 30;
      if (i < browser.count()) {
        selected = i;
        view = ACTIONS;
        action = 0;
        draw();
        return;
      }
    }
    bBack.pressedEdge = true;
  }
  if (bBack.pressed()) {
    if (view != LIST) {
      view = LIST;
      draw();
      return;
    }
    if (strcmp(browser.path(), "/")) {
      browser.parent();
      selected = top = 0;
      refresh();
      draw();
      return;
    }
    view=DISKS;disk=onUsb?1:0;draw();
    return;
  }
  if (!ready) {
    if (bOk.pressed() || usbInputHit(20, 230, 160, 28)) {
      refresh();
      draw();
    }
    return;
  }
  bool ok = bOk.pressed(), changed = false;
  if (view == LIST) {
    unsigned count = browser.count();
    if (!count) {
      if (ok) {
        refresh();
        draw();
      }
      return;
    }
    if (bUp.pressed()) {
      selected = selected ? selected - 1 : count - 1;
      changed = true;
    }
    if (bDown.pressed()) {
      selected = (selected + 1) % count;
      changed = true;
    }
    if (bLeft.pressed()) {
      selected = selected >= ROWS ? selected - ROWS : 0;
      changed = true;
    }
    if (bRight.pressed()) {
      selected = std::min(selected + ROWS, count - 1);
      changed = true;
    }
    if (usbInputHit(16, 84, 448, ROWS * 30)) {
      unsigned i = top + (usbPointer.y - 84) / 30;
      if (i < count) {
        selected = i;
        ok = true;
      }
    }
    if (changed) {
      top = selected / ROWS * ROWS;
      draw();
    }
    if (bFire.pressed() || usbInputHit(358, 268, 106, 28)) {
      view = ACTIONS;
      action = 0;
      draw();
      return;
    }
    if (ok)
      open();
  } else if (view == ACTIONS) {
    if (bUp.pressed()) {
      action = (action + 2) % 3;
      changed = true;
    }
    if (bDown.pressed()) {
      action = (action + 1) % 3;
      changed = true;
    }
    for (unsigned i = 0; i < 3; i++)
      if (usbInputHit(40, 126 + i * 42, 400, 28)) {
        action = i;
        ok = true;
      }
    if (changed)
      draw();
    if (ok) {
      if (action == 0)
        open();
      else {
        view = action == 1 ? DETAILS : DELETE_CONFIRM;
        confirmDelete = false;
        draw();
      }
    }
  } else if (view == DELETE_CONFIRM) {
    if (bLeft.pressed() || bRight.pressed()) {
      confirmDelete = !confirmDelete;
      draw();
    }
    if (usbInputHit(40, 218, 180, 28)) {
      confirmDelete = false;
      ok = true;
    }
    if (usbInputHit(260, 218, 180, 28)) {
      confirmDelete = true;
      ok = true;
    }
    if (ok) {
      if (confirmDelete && !browser.removeEntry(selected)) {
        error();
        return;
      }
      view = LIST;
      refresh();
      draw();
    }
  } else if (view == TEXT) {
    bool next = bRight.pressed() && more, prev = bLeft.pressed() && page;
    if (next || prev) {
      page += next ? 1 : -1;
      if (!browser.textPage(selected, page, preview, sizeof(preview), more)) {
        error();
        return;
      }
      draw();
    }
    if (ok) {
      view = ACTIONS;
      action = 0;
      draw();
    }
  } else if (ok) {
    view = LIST;
    draw();
  }
}
