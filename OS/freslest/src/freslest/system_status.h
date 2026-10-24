#pragma once
#include <Arduino.h>
#include <time.h>
void systemStatusBegin();
void systemStatusUpdate(uint32_t now);
void systemStatusDraw(bool force=false);
bool rtcGetLocal(struct tm& out);
bool rtcSetLocal(int year,int month,int day,int hour,int minute,int second);
void rtcSyncRequest();
bool dhtReading(float& temperature,float& humidity);
