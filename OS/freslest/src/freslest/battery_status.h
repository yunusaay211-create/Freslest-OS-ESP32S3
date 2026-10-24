#pragma once
inline int batteryPercentForVoltage(float voltage) {
  if(!(voltage>=2.5f&&voltage<=4.45f))return -1;
  int percent=(int)((voltage-3.3f)*100.0f/0.9f);
  return percent<0?0:percent>100?100:percent;
}
