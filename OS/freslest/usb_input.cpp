#include "usb_input.h"
#ifdef FRESLEST_USB_INPUT_HOST_TEST
#include "usb_input_host_platform.h"
#else
#include "gfx.h"
#include "screen_mirror.h"
#include "src/freslest/apps.h"
#include "src/freslest/audio_manager.h"
#include <Arduino.h>
#include <nvs.h>
#endif
#include <freslest_hid.h>
extern "C" {
#include <freslest_hid_state.h>
}
UsbPointer usbPointer;
namespace {
freslest_hid_state_t pcInput{};
bool viewerMode=false;
bool deviceMode = false, resumeNes = false, textMode = false, tasks = false;
uint8_t held = 0, pressed = 0, previous = 0, nesPending = 0;
uint32_t repeatAt = 0, charRepeatAt = 0;
uint32_t lastActivity = 0;
unsigned repeatUsage = 0;
char characters[32];
unsigned readAt = 0, writeAt = 0;
const char *specialKey = nullptr;
// This private merger leaves all four external slots available to BLE.
void mergedInput(freslest_hid_snapshot_t*out,bool consume){
 if(consume)freslest_hid_snapshot(out);else freslest_hid_peek(out);
 freslest_hid_snapshot_t pc{};freslest_hid_state_take(&pcInput,&pc,consume);
 for(unsigned i=0;i<32;++i){out->keys[i]|=pc.keys[i];out->pressed[i]|=pc.pressed[i];}
 out->mouse_buttons|=pc.mouse_buttons;out->mouse_pressed|=pc.mouse_pressed;
 out->keyboards+=pc.keyboards;out->mice+=pc.mice;
}
bool key(const uint8_t *k, unsigned u) { return freslest_hid_key(k, u); }
uint8_t uiMask(const uint8_t *k) {
  uint8_t m = 0;
  if (key(k, 0x52) || (!textMode && key(k, 0x1a)))
    m |= 1;
  if (key(k, 0x51) || (!textMode && key(k, 0x16)))
    m |= 2;
  if (key(k, 0x50) || (!textMode && key(k, 4)))
    m |= 4;
  if (key(k, 0x4f) || (!textMode && key(k, 7)))
    m |= 8;
  if (key(k, 0x28) || (!textMode && key(k, 0x2c)))
    m |= 16;
  if (key(k, 0x29))
    m |= 32;
  if (key(k, 0xe0) || key(k, 0xe4) || (!textMode && key(k, 9)))
    m |= 64;
  return m;
}
uint8_t nesMask(const uint8_t *k) {
  uint8_t m = 0;
  if (key(k, 0x1b))
    m |= 1;
  if (key(k, 0x1d))
    m |= 2;
  if (key(k, 0xe5))
    m |= 4;
  if (key(k, 0x28))
    m |= 8;
  if (key(k, 0x52) || key(k, 0x1a))
    m |= 16;
  if (key(k, 0x51) || key(k, 0x16))
    m |= 32;
  if (key(k, 0x50) || key(k, 4))
    m |= 64;
  if (key(k, 0x4f) || key(k, 7))
    m |= 128;
  return m;
}
int ascii(unsigned u, bool shift) {
  if (u >= 4 && u <= 29)
    return (shift ? 'A' : 'a') + u - 4;
  if (u >= 30 && u <= 39)
    return (shift ? "!@#$%^&*()" : "1234567890")[u - 30];
  if (u == 0x2a)
    return 8;
  if (u == 0x4c)
    return 127;
  if (u == 0x2c)
    return ' ';
  if (u >= 0x2d && u <= 0x38)
    return (shift ? "_+{}|~:\"~<>?" : "-=[]\\~;'`,./")[u - 0x2d];
  return -1;
}
void enqueue(int c) {
  if (c < 0)
    return;
  unsigned next = (writeAt + 1) % sizeof(characters);
  if (next != readAt) {
    characters[writeAt] = (char)c;
    writeAt = next;
  }
}
} // namespace
void usbInputPcKeys(const uint8_t keys[32]){
 if(!pcInput.slot[0].kind)freslest_hid_state_connect(&pcInput,0,1);
 freslest_hid_state_keys(&pcInput,0,keys);
}
void usbInputPcMouse(int x,int y,uint8_t buttons){
 if(!pcInput.slot[1].kind)freslest_hid_state_connect(&pcInput,1,2);
 usbPointer.x=x;usbPointer.y=y;freslest_hid_state_mouse(&pcInput,1,buttons,0,0);
}
void usbInputPcRelease(){
 freslest_hid_state_disconnect(&pcInput,0);freslest_hid_state_disconnect(&pcInput,1);
}
void usbInputBegin() {
  nvs_handle_t n;
  if (nvs_open("fl_usb", NVS_READWRITE, &n) == ESP_OK) {
    uint8_t pc = 0, resume = 0, screen=0;
    nvs_get_u8(n, "pc", &pc);
    nvs_get_u8(n, "nes", &resume);
    nvs_get_u8(n,"screen",&screen);
    deviceMode = pc;viewerMode=!pc&&screen;
    resumeNes = resume;
    if (resume) {
      nvs_set_u8(n, "nes", 0);
      nvs_commit(n);
    }
    nvs_close(n);
  }
  if(viewerMode)screenMirrorNativeBegin();
  if (usbInputHostMode() && !freslest_hid_start())
    Serial.println(freslest_hid_status());
}
bool usbInputViewerMode(){return viewerMode;}
bool usbInputHostMode(){return !deviceMode&&!viewerMode;}
bool usbInputSwitchViewer(bool enabled){
 nvs_handle_t n;esp_err_t e=nvs_open("fl_usb",NVS_READWRITE,&n);
 if(e==ESP_OK){
  e=nvs_set_u8(n,"screen",enabled?1:0);
  if(e==ESP_OK)e=nvs_set_u8(n,"pc",0);
  if(e==ESP_OK)e=nvs_set_u8(n,"nes",0);
  if(e==ESP_OK){e=nvs_commit(n);}
  nvs_close(n);
 }
 if(e!=ESP_OK)return false;
 audioStop();ESP.restart();return false;
}
bool usbInputDeviceMode() { return deviceMode; }
bool usbInputResumeNes() { return resumeNes; }
bool usbInputSwitchMode(bool pc) {
  if (pc == deviceMode && !viewerMode)
    return true;
  nvs_handle_t n;
  esp_err_t e = nvs_open("fl_usb", NVS_READWRITE, &n);
  if (e == ESP_OK) {
    e = nvs_set_u8(n,"screen",0);
    if(e==ESP_OK)e = nvs_set_u8(n, "pc", pc ? 1 : 0);
    if (e == ESP_OK)
      e = nvs_set_u8(n, "nes", (!pc&&deviceMode) ? 1 : 0);
    if (e == ESP_OK)
      e = nvs_commit(n);
    nvs_close(n);
  }
  if (e != ESP_OK) {
    Serial.printf("USB mode save failed: %s\n", esp_err_to_name(e));
    return false;
  }
  tft.fillScreen(0);
  drawHeader("USB mode");
  clearBody();
  tft.setTextColor(0xffff, 0);
  tft.setTextSize(2);
  tft.setCursor(24, 100);
  tft.print("Switching USB mode...");
  tft.setTextSize(1);
  tft.setCursor(24, 142);
  tft.print(pc ? "Connect USB/OTG to PC. UART keeps power."
               : "Reconnect keyboard / mouse to USB/OTG.");
  audioStop();
  gfxPresent();
  delay(300);
  ESP.restart();
  return false;
}
void usbInputUpdate(uint32_t now) {
  freslest_hid_snapshot_t s{};
  mergedInput(&s,true);
  bool active=s.dx || s.dy || s.mouse_buttons || s.mouse_pressed;
  for(unsigned i=0;i<sizeof(s.keys);++i)active|=s.keys[i]||s.pressed[i];
  if(active)lastActivity=now;
  usbPointer.visible = s.mice != 0;
  usbPointer.left = s.mouse_buttons & 1;
  usbPointer.click = s.mouse_pressed & 1;
  usbPointer.rightClick = s.mouse_pressed & 2;
  usbPointer.x = constrain(usbPointer.x + s.dx, 0, tft.width()-1);
  usbPointer.y = constrain(usbPointer.y + s.dy, 0, tft.height()-1);
  held = uiMask(s.keys);
  pressed = uiMask(s.pressed) | (held & ~previous);
  if (usbPointer.rightClick && app != APP_FILES && app != APP_BROWSER)
    pressed |= 32;
  if (held != previous)
    repeatAt = now + 350;
  else if ((int32_t)(now - repeatAt) >= 0) {
    pressed |= held & 15;
    repeatAt = now + 90;
  }
  previous = held;
  nesPending |= nesMask(s.pressed);
  tasks = key(s.pressed, 0x2b);
  specialKey=nullptr;
  if(app==APP_BROWSER&&textMode){
    bool shift=key(s.keys,0xe1)||key(s.keys,0xe5);
    if(tasks){specialKey=shift?"Shift+Tab":"Tab";tasks=false;}
    if(key(s.pressed,0x4a))specialKey="Home";
    if(key(s.pressed,0x4d))specialKey="End";
    if(key(s.pressed,0x4b))specialKey="PageUp";
    if(key(s.pressed,0x4e))specialKey="PageDown";
    if(key(s.keys,0xe0)||key(s.keys,0xe4)){
      if(key(s.pressed,4))specialKey="Control+a";
      if(key(s.pressed,6))specialKey="Control+c";
      if(key(s.pressed,0x19))specialKey="Control+v";
      return;
    }
  }
  if (!textMode)
    return;
  bool shift = key(s.keys, 0xe1) || key(s.keys, 0xe5);
  for (unsigned u = 4; u < 128; u++)
    if (key(s.pressed, u)) {
      int c = ascii(u, shift);
      enqueue(c);
      if (c >= 0) {
        repeatUsage = u;
        charRepeatAt = now + 400;
      }
    }
  if (repeatUsage && !key(s.keys, repeatUsage))
    repeatUsage = 0;
  if (repeatUsage && (int32_t)(now - charRepeatAt) >= 0) {
    enqueue(ascii(repeatUsage, shift));
    charRepeatAt = now + 70;
  }
}
uint8_t usbInputHeld() { return held; }
uint8_t usbInputPressed() { return pressed; }
uint32_t usbInputLastActivity() { return lastActivity; }
uint8_t usbInputNesPad() {
  freslest_hid_snapshot_t s{};
  mergedInput(&s,false);
  uint8_t result = nesMask(s.keys) | nesPending;
  nesPending = 0;
  return result;
}
bool usbInputTasks() { return tasks; }
void usbInputTextMode(bool on) {
  textMode = on;
  readAt = writeAt = 0;
  repeatUsage = 0;
}
int usbInputCharacter() {
  if (readAt == writeAt)
    return -1;
  int c = (unsigned char)characters[readAt];
  readAt = (readAt + 1) % sizeof(characters);
  return c;
}
const char *usbInputSpecialKey(){return specialKey;}
bool usbInputHit(int x, int y, int w, int h) {
  return usbPointer.visible && usbPointer.click && usbPointer.x >= x &&
         usbPointer.x < x + w && usbPointer.y >= y && usbPointer.y < y + h;
}
