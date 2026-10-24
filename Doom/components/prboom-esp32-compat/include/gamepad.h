#ifndef GAMEPAD_H
#define GAMEPAD_H

void gamepadInit(void);
void gamepadPoll(void);
int gamepadTakeWeapon(void); /* -1: none; otherwise an owned Doom weapon ID */


#endif
