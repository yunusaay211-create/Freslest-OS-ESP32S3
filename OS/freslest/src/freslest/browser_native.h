#pragma once
#include "browser_html.h"
constexpr size_t NATIVE_JPEG_MAX = 98304;
struct NativePage {
  web_lite::Document document;
  uint8_t jpeg[NATIVE_JPEG_MAX];
  size_t jpegLength;
  char message[120];
  int httpStatus;
  bool image;
};
struct NativeBrowserStatus {
  bool busy;
  uint32_t revision;
  char message[120];
};
bool nativeBrowserOpen(const char *url);
void nativeBrowserCancel();
NativePage *nativeBrowserTake(int &slot);
void nativeBrowserRelease(int slot);
NativeBrowserStatus nativeBrowserStatus();
