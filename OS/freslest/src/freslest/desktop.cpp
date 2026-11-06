#include "desktop.h"
#include "../../gfx.h"
#include "../../input.h"
#include "../../usb_input.h"
#include "apps.h"
#include "ble_advertising.h"
#include "install_notice.h"
#include "system_status.h"
#include "text_entry.h"
#include "usb_storage.h"
#include <WiFi.h>
#include <esp_app_desc.h>
#include <freslest_hid.h>
#include <freslest_video.h>
#include <vga_pixels.h>

namespace {
constexpr int Width = VGA_WIDTH, Height = VGA_HEIGHT;
constexpr int TaskY = Height - 28, Title = 22, Frame = 2;
constexpr int AppWidth = 480, AppHeight = 320;
constexpr int WinWidth = AppWidth + Frame * 2, WinHeight = AppHeight + Title + Frame;
constexpr int MaxWindows = 4, DesktopIcons = 6;
constexpr uint16_t Blue = 0x125d, Dark = 0x08cd, White = 0xffff, Light = 0xd6df;
struct Shortcut { const char *name; AppId app; uint16_t color; };
constexpr Shortcut shortcuts[] = {
    {"Dosyalar", APP_FILES, 0xff03}, {"Ayarlar", APP_SETTINGS, 0x9e5f},
    {"FreStore", APP_FRESTORE, 0x1dff}, {"Terminal", APP_TERMINAL, 0x52aa},
    {"Oyunlar", APP_GAME_MENU, 0x5766}, {"Yuklu Uyg.", APP_FRESTORE_APPS, 0x8d9f},
    {"Uygulamalar", APP_APPS, 0xe58f}, {"Guncelleme", APP_FRESLEST_UPDATE, 0x764c},
    {"Sistem Bilgisi", APP_SYSINFO, 0xd6df}, {"Test", APP_TEST, 0xf565}
};
constexpr int Count = sizeof(shortcuts) / sizeof(*shortcuts);
constexpr int MenuTop = TaskY - Count * 22 - 8;

struct Window { int shortcut, x, y; AppId page; };
Window windows[MaxWindows];
int count = 0, selection = 0, lastIcon = -1;
int dragX = 0, dragY = 0;
uint32_t iconAt = 0, statusAt = 0;
bool dirty = true, menu = false, dragging = false, ready = false, windowLimit = false;
const char *noticeShown = nullptr;
char statusLine[32] = "";
lgfx::LGFX_Sprite canvas;

bool contains(int x, int y, int w, int h) {
    return usbPointer.x >= x && usbPointer.x < x + w &&
           usbPointer.y >= y && usbPointer.y < y + h;
}
void label(int x, int y, const char *text, uint16_t color = White, int scale = 1) {
    canvas.setTextColor(color);
    canvas.setTextSize(scale);
    canvas.setCursor(x, y);
    canvas.print(text);
}
bool fullScreen(AppId page) {
    switch (page) {
    case APP_CAMERA: case APP_USB_TRANSFER: case APP_FRESLEST_UPDATE:
    case APP_DINO: case APP_TETRIS: case APP_SNAKE: case APP_MINES:
    case APP_2048: case APP_DOOM: case APP_NES_PLAYER: return true;
    default: return false;
    }
}
void rememberPage() {
    if (count && app != APP_MENU) windows[count - 1].page = app;
}
void focus(int index) {
    if (index == count - 1) return;
    rememberPage();
    const Window chosen = windows[index];
    for (int i = index; i < count - 1; ++i) windows[i] = windows[i + 1];
    windows[count - 1] = chosen;
    enterApp(chosen.page);
    dirty = true;
}
void closeTop() {
    if (count) --count;
    enterApp(count ? windows[count - 1].page : APP_MENU);
    dragging = false;
    windowLimit = false;
    dirty = true;
}
void open(int shortcut) {
    menu = false;
    dirty = true;
    lastIcon = -1;
    for (int i = 0; i < count; ++i) {
        if (windows[i].shortcut == shortcut) {
            focus(i);
            return;
        }
    }
    if (count == MaxWindows) {
        windowLimit = true;
        return;
    }
    rememberPage();
    const int offset = count * 16;
    const auto id = shortcuts[shortcut].app;
    windows[count++] = {shortcut, 74 + offset, 34 + offset, id};
    enterApp(id);
}
void updateStatus(uint32_t now) {
    if (now - statusAt < 250) return;
    statusAt = now;
    struct tm clock{};
    char time[6] = "--:--";
    if (rtcGetLocal(clock)) strftime(time, sizeof(time), "%H:%M", &clock);
    char line[sizeof(statusLine)];
    snprintf(line, sizeof(line), "%s %s %s %s  %s",
             WiFi.status() == WL_CONNECTED ? "WiFi" : "----",
             freslestBleInfo().connected ? "BT" : "--",
             soundEnabled ? "SES" : "---",
             usbInputAttached() || usbStorageInfo().present() ? "USB" : "---", time);
    if (strcmp(statusLine, line)) {
        strcpy(statusLine, line);
        dirty = true;
    }
}
bool handleShell(uint32_t now) {
    if (dragging) {
        if (!usbPointer.left || !usbPointer.visible) dragging = false;
        else {
            auto &w = windows[count - 1];
            const int x = constrain(usbPointer.x - dragX, 0, Width - WinWidth);
            const int y = constrain(usbPointer.y - dragY, 0, TaskY - WinHeight);
            if (w.x != x || w.y != y) { w.x = x; w.y = y; dirty = true; }
        }
        return true;
    }
    if (usbPointer.visible && usbPointer.click && contains(0, TaskY, 86, 28)) {
        menu = !menu; dirty = true; return true;
    }
    if (menu || !count) {
        const int length = menu ? Count : DesktopIcons;
        int move = bDown.pressed() || bRight.pressed() ? 1 :
                   bUp.pressed() || bLeft.pressed() ? -1 : 0;
        if (menu && usbPointer.wheel) move = usbPointer.wheel > 0 ? -1 : 1;
        if (move) {
            selection = (selection + move + length) % length;
            dirty = true;
        }
        if (bOk.pressed()) { open(selection % length); return true; }
        if (menu && bBack.pressed()) { menu = false; dirty = true; return true; }
    }
    if (!textEntryActive() && usbInputTasks()) {
        if (count > 1 && !menu) focus(0);
        else { menu = !menu; dirty = true; }
        return true;
    }
    if (!usbPointer.visible || !usbPointer.click) return menu || !count;
    if (menu) {
        if (contains(0, MenuTop + 4, 170, Count * 22))
            open((usbPointer.y - MenuTop - 4) / 22);
        else { menu = false; dirty = true; }
        return true;
    }
    if (contains(86, TaskY, count * 86, 28)) {
        focus((usbPointer.x - 86) / 86);
        return true;
    }
    for (int i = count - 1; i >= 0; --i) {
        const auto w = windows[i];
        if (!contains(w.x, w.y, WinWidth, WinHeight)) continue;
        const bool wasFocused = i == count - 1;
        focus(i);
        if (contains(w.x + WinWidth - 23, w.y, 23, Title)) { closeTop(); return true; }
        if (contains(w.x, w.y, WinWidth, Title)) {
            dragX = usbPointer.x - w.x; dragY = usbPointer.y - w.y;
            dragging = usbPointer.left;
            return true;
        }
        return !wasFocused;
    }
    for (int i = 0; i < DesktopIcons; ++i) {
        if (!contains(10, 14 + i * 64, 60, 58)) continue;
        if (lastIcon == i && now - iconAt <= 400) open(i);
        else { selection = lastIcon = i; iconAt = now; dirty = true; }
        return true;
    }
    lastIcon = -1;
    return true;
}

char terminalLine[32], terminalReply[76];
unsigned terminalLength = 0;
void drawTerminal() {
    tft.fillScreen(0x0843);
    drawHeader("Terminal");
    drawFooter("ENTER Komut   help", "BACK Kapat");
    tft.setTextSize(1);
    tft.setTextColor(0xbff8);
    tft.setCursor(18, 65); tft.print(terminalReply);
    tft.setCursor(18, 92); tft.printf("> %s_", terminalLine);
}
} // namespace

void desktopBegin() {
    if (freslest_video_output() != FRESLEST_DISPLAY_VGA) return;
    canvas.setPsram(true);
    canvas.setColorDepth(16);
    ready = canvas.createSprite(Width, Height) != nullptr;
    if (ready) {
        canvas.setTextWrap(false);
        canvas.setSwapBytes(false);
    } else Serial.println("Desktop framebuffer unavailable; VGA legacy menu active.");
}
bool desktopVisible() { return ready && !fullScreen(app) && (app == APP_MENU || count); }
bool desktopHasWindow() { return count && app != APP_MENU; }
const uint16_t *desktopPixels() { return static_cast<const uint16_t *>(canvas.getBuffer()); }

void desktopUpdate(uint32_t now) {
    updateStatus(now);
    if (handleShell(now) || !desktopHasWindow()) return;
    const auto w = windows[count - 1];
    const UsbPointer saved = usbPointer;
    usbPointer.visible &= contains(w.x + Frame, w.y + Title, AppWidth, AppHeight);
    usbPointer.x -= w.x + Frame;
    usbPointer.y -= w.y + Title;
    if (!usbPointer.visible) {
        usbPointer.left = usbPointer.click = usbPointer.rightClick = false;
        usbPointer.wheel = 0;
        if (saved.rightClick && app != APP_FILES && app != APP_BROWSER) bBack.pressedEdge = false;
    }
    appLoop(now);
    usbPointer = saved;
    if (app == APP_MENU) closeTop();
    else rememberPage();
}

bool desktopCompose(const uint16_t *appPixels, bool appChanged) {
    const char *notice = installNoticeText();
    if (!dirty && !appChanged && notice == noticeShown) return false;
    noticeShown = notice;
    dirty = false;
    canvas.fillScreen(0x24be);
    canvas.fillCircle(536, 87, 38, 0xffc9);
    canvas.fillTriangle(0, 355, 385, 213, 640, 418, 0x3d83);
    canvas.fillTriangle(0, 406, 590, 270, 640, 451, 0x2505);
    canvas.fillRect(0, 416, Width, TaskY - 416, 0x2505);
    label(440, 24, "FRESLEST DESKTOP", White, 2);
    for (int i = 0; i < DesktopIcons; ++i) {
        const int y = 14 + i * 64;
        if (selection == i) canvas.fillRoundRect(10, y, 60, 58, 4, Blue);
        canvas.fillRoundRect(23, y + 5, 32, 28, 4, shortcuts[i].color);
        label(32, y + 10, i == 3 ? ">" : "+", Dark, 2);
        label(12, y + 41, shortcuts[i].name);
    }
    for (int i = 0; i < count; ++i) {
        const auto &w = windows[i];
        const bool active = i == count - 1 && app != APP_MENU;
        canvas.fillRect(w.x, w.y, WinWidth, WinHeight, active ? Blue : Dark);
        canvas.fillRect(w.x + Frame, w.y + Title, AppWidth, AppHeight, Light);
        if (active) canvas.pushImage(w.x + Frame, w.y + Title, AppWidth, AppHeight, appPixels);
        else label(w.x + 18, w.y + Title + 16, "Etkinlestirmek icin tikla", Dark);
        label(w.x + 8, w.y + 7, shortcuts[w.shortcut].name);
        canvas.fillRoundRect(w.x + WinWidth - 21, w.y + 3, 17, 16, 3, 0xd185);
        label(w.x + WinWidth - 16, w.y + 7, "X");
    }
    canvas.fillRect(0, TaskY, Width, Height - TaskY, Blue);
    canvas.fillRoundRect(2, TaskY + 2, 80, 24, 4, 0x3585);
    label(15, TaskY + 10, "BASLAT");
    for (int i = 0; i < count; ++i) {
        const int x = 86 + i * 86;
        canvas.fillRoundRect(x + 2, TaskY + 3, 82, 22, 3, i == count - 1 ? Dark : 0x249f);
        label(x + 6, TaskY + 10, shortcuts[windows[i].shortcut].name);
    }
    label(Width - 157, TaskY + 10, statusLine);
    if (notice) label(440, 45, notice);
    else if (windowLimit) label(440, 45, "4 pencere acik. Birini kapat.");
    if (menu) {
        canvas.fillRect(0, MenuTop, 170, TaskY - MenuTop, Dark);
        for (int i = 0; i < Count; ++i) {
            const int y = MenuTop + 4 + i * 22;
            if (i == selection) canvas.fillRect(4, y, 162, 22, Blue);
            canvas.fillRect(10, y + 5, 11, 11, shortcuts[i].color);
            label(29, y + 7, shortcuts[i].name);
        }
    }
    return true;
}

void desktopTerminalEnter() {
    terminalLength = 0; terminalLine[0] = 0;
    strcpy(terminalReply, "Freslest Terminal. help yazin.");
    usbInputTextMode(true);
    drawTerminal();
}
void desktopTerminalLoop() {
    if (bBack.pressed()) { backToMenu(); return; }
    bool changed = false;
    for (int c; (c = usbInputCharacter()) >= 0;) {
        if (c == 8 && terminalLength) --terminalLength;
        else if (c >= 32 && c < 127 && terminalLength + 1 < sizeof(terminalLine))
            terminalLine[terminalLength++] = char(c);
        terminalLine[terminalLength] = 0;
        changed = true;
    }
    if (bOk.pressed()) {
        if (!strcmp(terminalLine, "help")) strcpy(terminalReply, "help  info  usb  vga");
        else if (!strcmp(terminalLine, "info"))
            snprintf(terminalReply, sizeof(terminalReply), "Freslest OS %s / ESP32-S3", esp_app_get_description()->version);
        else if (!strcmp(terminalLine, "usb")) snprintf(terminalReply, sizeof(terminalReply), "%s", freslest_hid_status());
        else if (!strcmp(terminalLine, "vga")) {
            freslest_video_diagnostics(); strcpy(terminalReply, "VGA tani bilgisi seri porta yazildi.");
        } else strcpy(terminalReply, "Komut bulunamadi. help yazin.");
        terminalLength = 0; terminalLine[0] = 0; changed = true;
    }
    if (changed) drawTerminal();
}
