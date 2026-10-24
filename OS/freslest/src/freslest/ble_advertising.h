#pragma once
#include <stdint.h>
struct FreslestBleDevice {
  uint8_t address[6], addressType;
  bool connected;
  char name[38];
  bool remembered;
};
struct FreslestBleInfo {
  bool enabled, ready, scanning, busy;
  uint8_t count, connected, pairMode;
  uint32_t revision, passkey;
  char status[76];
  FreslestBleDevice devices[6];
  uint32_t attempts, disconnects;
  int lastCloseReason;
};
void freslestBleStart();
void freslestBleStop();
void freslestBleScan();
void freslestBleConnect(unsigned index);
void freslestBleForget();
void freslestBlePairReply(bool accept);
FreslestBleInfo freslestBleInfo();
void freslestBleDiagnostics();
