#include "battery_status.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
 assert(batteryPercentForVoltage(0)==-1);
 assert(batteryPercentForVoltage(5)==-1);
 assert(batteryPercentForVoltage(NAN)==-1);
 assert(batteryPercentForVoltage(-1)==-1);
 assert(batteryPercentForVoltage(3.0f)==0);
 assert(batteryPercentForVoltage(3.75f)>=49&&batteryPercentForVoltage(3.75f)<=50);
 assert(batteryPercentForVoltage(4.2f)>=99);
 puts("PASS: absent/invalid/USB-level battery readings select X, valid cell voltage selects bounded percentage");
}
