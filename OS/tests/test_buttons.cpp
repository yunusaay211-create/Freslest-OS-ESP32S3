#include <Arduino.h>
#include "../freslest/input.h"
#include <cassert>
#include <cstdio>
uint32_t testMillis=0;static unsigned clicks=0;
void audioClick(){++clicks;}
extern void handleGlobalClick();
int main(){
 for(int& pin:testPins){pin=HIGH;}
 inputBegin();inputUpdate(0);assert(!bUp.down()&&!bFire.down());
 testPins[BTN_UP]=LOW;inputUpdate(1);inputUpdate(20);assert(!bUp.down());
 testPins[BTN_UP]=HIGH;inputUpdate(21);testPins[BTN_UP]=LOW;inputUpdate(22);inputUpdate(47);assert(bUp.down()&&bUp.pressed());
 handleGlobalClick();assert(clicks==1);inputUpdate(48);assert(bUp.down()&&!bUp.pressed());
 testPins[BTN_UP]=HIGH;inputUpdate(49);inputUpdate(74);assert(!bUp.down()&&!bUp.pressed());
 testPins[BTN_FIRE]=LOW;inputUpdate(75);inputUpdate(100);assert(bFire.down()&&bFire.pressed());inputUpdate(101);assert(bFire.down()&&!bFire.pressed());
 testPins[BTN_OK]=LOW;testPins[BTN_DOWN]=LOW;inputUpdate(102);inputUpdate(127);assert(bOk.down()&&bDown.down());
 puts("PASS: physical button debounce, bounce rejection, one-shot edges, FIRE and simultaneous OK/direction");
}
