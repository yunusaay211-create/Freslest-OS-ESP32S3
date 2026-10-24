#include "freslest_hid_state.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 freslest_hid_state_t d={0};freslest_hid_snapshot_t s;freslest_hid_state_connect(&d,0,1);freslest_hid_state_connect(&d,1,2);
 uint8_t press[]={0x22,0,0x1a,7,0,0,0,0},release[8]={0};assert(freslest_hid_state_report(&d,0,press,8));assert(freslest_hid_state_report(&d,0,release,8));freslest_hid_state_take(&d,&s,true);assert(!freslest_hid_key(s.keys,0x1a)&&freslest_hid_key(s.pressed,0x1a)&&freslest_hid_key(s.pressed,0xe5));freslest_hid_state_take(&d,&s,true);assert(!freslest_hid_key(s.pressed,0x1a));
 freslest_hid_state_report(&d,0,press,8);uint8_t rollover[]={0,0,1,1,1,1,1,1};assert(!freslest_hid_state_report(&d,0,rollover,8));assert(!freslest_hid_state_report(&d,0,press,7));freslest_hid_state_take(&d,&s,true);assert(freslest_hid_key(s.keys,0x1a));
 uint8_t mouse[]={1,8,255},up[]={0,0,0};for(int i=0;i<1000;i++)freslest_hid_state_report(&d,1,mouse,3);freslest_hid_state_report(&d,1,up,3);freslest_hid_state_take(&d,&s,true);assert(s.dx==2047&&s.dy==-1000&&s.mouse_pressed==1&&!s.mouse_buttons);
 freslest_hid_state_connect(&d,2,1);freslest_hid_state_report(&d,2,press,8);freslest_hid_state_disconnect(&d,0);freslest_hid_state_take(&d,&s,true);assert(s.keyboards==1&&freslest_hid_key(s.keys,0x1a));freslest_hid_state_disconnect(&d,2);freslest_hid_state_disconnect(&d,1);freslest_hid_state_take(&d,&s,true);assert(!s.keyboards&&!s.mice&&!s.dx&&!s.mouse_buttons&&!freslest_hid_key(s.keys,0x1a));
 puts("PASS: boot HID decoder, composite slots, tap latch, modifiers, rollover/malformed reports, bounded mouse motion and unplug releases");
}
