#pragma once
#include <stdint.h>
enum class FreslestBootStage : uint8_t {
  Boot, Storage, Display, Settings, Usb, Menu, Wifi, Bluetooth, Ready, Sleep
};
void freslestSafetyBegin();
void freslestSafetyStage(FreslestBootStage stage);
void freslestSafetyError(const char *message);
bool freslestRecoveryRequested();
// Physical buttons only; this function never initializes USB, Wi-Fi or BT.
void freslestRecoveryRun(const char *error = nullptr);
void freslestSafetyTick(uint32_t now, bool healthy);
void freslestIdleUpdate(uint32_t now);
