#pragma once
#include <stdint.h>
void wifiManagerBegin();
void wifiManagerAllowStartup();
void wifiManagerUpdate(uint32_t now);
void wifiManagerRequest();
void wifiManagerSetEnabled(bool on);
bool wifiPortalActive();
void wifiManagerDiagnostics();
