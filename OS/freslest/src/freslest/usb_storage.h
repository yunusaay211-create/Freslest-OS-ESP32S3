#pragma once
#include <stdint.h>
enum class UsbStorageState : uint8_t { Disabled, Absent, Mounting, Ready, Error, Ejected };
struct UsbStorageInfo {
  UsbStorageState state=UsbStorageState::Disabled;
  uint32_t revision=0;
  uint64_t bytes=0;
  char message[80]="USB storage not started";
  bool present() const { return state!=UsbStorageState::Disabled&&state!=UsbStorageState::Absent; }
};
// Reuses the HID service's host library/daemon, never starts a second USB host.
bool usbStorageBegin();
UsbStorageInfo usbStorageInfo();
const char *usbStorageRoot();
bool usbStorageRetry();
bool usbStorageEject();
// Hold for every USB directory/read/delete operation. Unmount waits for users;
// a disconnect callback only publishes removal and queues cleanup.
bool usbStorageLock();
void usbStorageUnlock();
// Atomically prevents new mounts before deep sleep. Refuses mounted/busy media.
bool usbStoragePrepareSleep();
