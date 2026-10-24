// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// Three interrupted boots are counted at the following startup. A clean
// software handoff or deep-sleep wake does not count as a failed boot.
namespace freslest_safe {
struct BootPolicy {
  uint8_t failures = 0, armed = 0, recovery = 0;
  bool begin(bool sleepWake, bool crash) {
    if (sleepWake) failures = 0;
    else if (armed || crash) { if (failures < 3) ++failures; }
    else failures = 0;
    if (failures >= 3) recovery = 1;
    armed = 1;
    return recovery;
  }
  void healthy() { failures = 0; armed = 0; }
  void cleanExit() { armed = 0; }
  void resume() { failures = 0; armed = 1; recovery = 0; }
};
struct IdlePolicy {
  static constexpr uint32_t timeout = 300000;
  uint32_t last = 0;
  void begin(uint32_t now) { last = now; }
  bool tick(uint32_t now, bool activity, bool blocked) {
    // A blocked operation receives a fresh five minutes when it finishes.
    if (activity || blocked) last = now;
    return (uint32_t)(now - last) >= timeout;
  }
};
}
