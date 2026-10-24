#pragma once
#include <stdint.h>
struct UsbPointer {
  int x = 240, y = 160;
  bool visible = false, left = false, click = false, rightClick = false;
};
extern UsbPointer usbPointer;
void usbInputBegin();
void usbInputUpdate(uint32_t now);
uint8_t usbInputHeld();
uint8_t usbInputPressed();
uint32_t usbInputLastActivity();
uint8_t usbInputNesPad();
bool usbInputTasks();
void usbInputTextMode(bool);
int usbInputCharacter();
const char *usbInputSpecialKey();
bool usbInputHit(int x, int y, int w, int h);
bool usbInputDeviceMode();
bool usbInputResumeNes();
bool usbInputSwitchMode(bool pc);

bool usbInputViewerMode();
bool usbInputHostMode();
bool usbInputSwitchViewer(bool enabled);

// PC input belongs to the UI task and never occupies a USB/BLE source slot.
void usbInputPcKeys(const uint8_t keys[32]);
void usbInputPcMouse(int x,int y,uint8_t buttons);
void usbInputPcRelease();
