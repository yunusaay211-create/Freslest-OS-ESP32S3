#include <assert.h>
#include <stdio.h>
#include "host_idf.h"
int64_t host_now;
unsigned host_delay_calls,host_pwm_updates;
int host_gpio[64],host_ledc_fail;
uint32_t host_duty,host_freq;
#include "../components/prboom-esp32-compat/gamepad.c"
#include "freslest_hid_state.h"
static freslest_hid_state_t usbState;
void freslest_hid_snapshot(freslest_hid_snapshot_t* out){freslest_hid_state_take(&usbState,out,true);}
#include "../components/prboom-esp32-compat/usb_doom.c"
int key_strafeleft=20,key_straferight=21,key_menu_up=11,key_menu_down=12,key_menu_left=13,key_menu_right=14;
int key_map=22,key_speed=23,key_strafe=24;

int key_up=11,key_down=12,key_left=13,key_right=14,key_use=15,key_escape=16,key_fire=17;
int key_menu_enter=18,key_menu_backspace=19;
player_t players[MAXPLAYERS];
int consoleplayer;
boolean menuactive,paused;
gamestate_t gamestate=GS_LEVEL;
GameMode_t gamemode=registered;
static int mouseMotion,hoverX,hoverY;
void Freslest_MenuPointer(int x,int y){hoverX=x;hoverY=y;}
static int held[512],presses[512],releases[512],drop_release,parent;
void D_PostEvent(event_t *e)
{
 if(e->type==ev_mouse){mouseMotion+=e->data2;return;}
 assert(e->data1>=0&&e->data1<512);
 if(e->type==ev_keydown){held[e->data1]=1;++presses[e->data1];}
 else if(e->type==ev_keyup){++releases[e->data1];if(drop_release==e->data1)drop_release=0;else held[e->data1]=0;}
}
int Freslest_MenuHasParent(void){return menuactive&&parent;}
static void edge(int pin,int pressed)
{
 host_gpio[pin]=!pressed;gamepadPoll();host_now+=30000;gamepadPoll();
}
static void reset(void)
{
 memset(&usbState,0,sizeof(usbState));memset(desired,0,sizeof(desired));memset(previous,0,sizeof(previous));mouseMotion=0;turnRemainder=0;pointerVisible=false;pointerX=160;pointerY=120;
 host_now=0;for(int i=0;i<64;++i)host_gpio[i]=1;
 memset(held,0,sizeof(held));memset(presses,0,sizeof(presses));memset(releases,0,sizeof(releases));
 memset(players,0,sizeof(players));players[0].playerstate=PST_LIVE;
 players[0].readyweapon=wp_pistol;players[0].pendingweapon=wp_nochange;
 players[0].weaponowned[wp_fist]=players[0].weaponowned[wp_pistol]=1;
 players[0].weaponowned[wp_shotgun]=players[0].weaponowned[wp_chaingun]=1;
 menuactive=paused=0;parent=0;gamestate=GS_LEVEL;gamemode=registered;drop_release=0;gamepadInit();
}
static void apply_weapon(int expected)
{
 int got=gamepadTakeWeapon();assert(got==expected);players[0].readyweapon=got;
}
int main(void)
{
 reset();edge(BTN_OK,1);assert(!held[key_use]);edge(BTN_RIGHT,1);apply_weapon(wp_shotgun);
 assert(!held[key_right]&&!held[key_use]);edge(BTN_RIGHT,0);edge(BTN_UP,1);apply_weapon(wp_chaingun);
 edge(BTN_UP,0);edge(BTN_LEFT,1);apply_weapon(wp_shotgun);edge(BTN_LEFT,0);
 edge(BTN_DOWN,1);apply_weapon(wp_pistol);edge(BTN_DOWN,0);edge(BTN_OK,0);assert(!held[key_use]);
 puts("PASS: all four OK+direction chords, only owned weapons, no movement/door events");
 reset();edge(BTN_RIGHT,1);assert(held[key_right]);edge(BTN_OK,1);assert(!held[key_right]);apply_weapon(wp_shotgun);
 edge(BTN_RIGHT,0);edge(BTN_OK,0);assert(!held[key_use]);
 reset();edge(BTN_OK,1);edge(BTN_OK,0);assert(held[key_use]);host_now+=90000;gamepadPoll();assert(!held[key_use]);
 puts("PASS: direction held before OK; OK alone uses door on release");
 reset();menuactive=1;edge(BTN_OK,1);assert(held[key_menu_enter]);edge(BTN_OK,0);assert(!held[key_menu_enter]);
 parent=1;edge(BTN_BACK,1);assert(held[key_menu_backspace]&&!held[key_escape]);parent=0;edge(BTN_BACK,0);assert(!held[key_menu_backspace]);
 edge(BTN_BACK,1);assert(held[key_escape]);edge(BTN_BACK,0);assert(!held[key_escape]);
 puts("PASS: menu confirmation and parent BACK release pairing");
 reset();edge(BTN_FIRE,1);drop_release=key_fire;edge(BTN_FIRE,0);assert(held[key_fire]);
 host_now=4999999;gamepadPoll();assert(held[key_fire]);host_now=5000000;gamepadPoll();assert(!held[key_fire]);
 edge(BTN_FIRE,1);int ups=releases[key_fire],downs=presses[key_fire];host_now=10000000;gamepadPoll();
 assert(held[key_fire]&&releases[key_fire]==ups&&presses[key_fire]==downs);
 held[key_up]=1;host_now=15000000;gamepadPoll();assert(!held[key_up]&&held[key_fire]);
 puts("PASS: 5-second repair recovers lost KEYUP; held fire is continuous");
 reset();edge(BTN_FIRE,1);edge(BTN_BACK,1);edge(BTN_OK,1);assert(!held[key_fire]);
 host_now+=1500000;gamepadPoll();assert(combo_blocked);edge(BTN_BACK,0);assert(combo_active);
 edge(BTN_OK,0);assert(!combo_active);gamepadPoll();host_now+=30000;gamepadPoll();assert(held[key_fire]);
 puts("PASS: BACK+OK manual reset and both-key release latch");
 reset();freslest_hid_state_connect(&usbState,0,1);freslest_hid_state_connect(&usbState,1,2);
 uint8_t keys[]={2,0,0x1a,7,0,0,0,0},up[8]={0};freslest_hid_state_report(&usbState,0,keys,8);gamepadPoll();assert(held[key_up]&&held[key_straferight]&&held[key_speed]);host_now=6000000;gamepadPoll();assert(held[key_up]);
 uint8_t mouse[]={1,12,0},mouseUp[]={0,0,0};edge(BTN_FIRE,1);freslest_hid_state_report(&usbState,1,mouse,3);gamepadPoll();assert(mouseMotion==3);edge(BTN_FIRE,0);assert(held[key_fire]);freslest_hid_state_report(&usbState,1,mouseUp,3);gamepadPoll();assert(!held[key_fire]&&held[key_up]);
 freslest_hid_state_disconnect(&usbState,0);gamepadPoll();assert(!held[key_up]&&!held[key_speed]);freslest_hid_state_connect(&usbState,0,1);freslest_hid_state_report(&usbState,0,keys,8);freslest_hid_state_report(&usbState,0,up,8);gamepadPoll();assert(held[key_up]);gamepadPoll();assert(!held[key_up]);
 menuactive=1;freslest_hid_state_report(&usbState,1,mouse,3);gamepadPoll();assert(held[key_menu_enter]&&!held[key_fire]);freslest_hid_state_disconnect(&usbState,1);gamepadPoll();assert(!held[key_menu_enter]);
 puts("PASS: actual HID decoder to Doom WASD/Shift, mouse turn/fire, physical+USB holds, tap latch, menu click and unplug release");
 reset();freslest_hid_state_connect(&usbState,1,2);
 uint8_t tiny[]={0,1,0};for(int i=0;i<3;i++){freslest_hid_state_report(&usbState,1,tiny,3);usbDoomPoll();assert(mouseMotion==0);}
 freslest_hid_state_report(&usbState,1,tiny,3);usbDoomPoll();assert(mouseMotion==1);
 tiny[1]=255;for(int i=0;i<4;i++){freslest_hid_state_report(&usbState,1,tiny,3);usbDoomPoll();}assert(mouseMotion==0);
 menuactive=1;uint8_t motion[]={0,12,8};freslest_hid_state_report(&usbState,1,motion,3);usbDoomPoll();
 int px,py;assert(usbDoomPointer(&px,&py)&&px==172&&py==128&&hoverX==px&&hoverY==py);
 menuactive=0;usbDoomPoll();assert(!usbDoomPointer(&px,&py));
 freslest_hid_state_disconnect(&usbState,1);usbDoomPoll();assert(!pointerVisible&&!turnRemainder);
 puts("PASS: quarter-rate mouse turning, fractional slow movement, reversal, menu sprite motion, game/unplug hiding");
 return 0;
}
