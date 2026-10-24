#pragma once
#include <Arduino.h>

// ESP32-S3 ADC pin for battery divider midpoint
#ifndef ADC_BAT
#define ADC_BAT 1
#endif

float readBatteryVoltage();
int batteryPercent(float v);
void drawBatteryTopBar(bool force=false);
