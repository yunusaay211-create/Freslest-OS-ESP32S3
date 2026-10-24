#include "freslest_hid_map.h"
#include "freslest_hid_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static const uint8_t keyboard[]={
 0x05,1,0x09,6,0xa1,1,0x85,1,0x05,7,0x19,0xe0,0x29,0xe7,
 0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x75,8,0x95,1,0x81,1,
 0x19,0,0x29,0x65,0x15,0,0x25,0x65,0x75,8,0x95,6,0x81,0,0xc0};
static const uint8_t mouse[]={
 0x05,1,0x09,2,0xa1,1,0x85,2,0x05,9,0x19,1,0x29,3,0x15,0,0x25,1,
 0x75,1,0x95,3,0x81,2,0x75,5,0x95,1,0x81,1,0x05,1,0x09,0x30,0x09,0x31,
 0x16,0,0xf8,0x26,0xff,7,0x75,12,0x95,2,0x81,6,0xc0};
int main(void){
 freslest_hid_map_t k,m;freslest_hid_decoded_t d;
 assert(freslest_hid_map_parse(&k,keyboard,sizeof(keyboard))&&k.kind==1);
 uint8_t key[]={0x22,0,0x1a,7,0,0,0,0};
 assert(freslest_hid_map_decode(&k,1,key,8,&d)&&d.kind==1);
 assert(freslest_hid_key(d.keys,0x1a)&&freslest_hid_key(d.keys,7)&&freslest_hid_key(d.keys,0xe1)&&freslest_hid_key(d.keys,0xe5));
 assert(!freslest_hid_map_decode(&k,1,key,7,&d)&&!freslest_hid_map_decode(&k,0,key,8,&d));
 key[2]=1;assert(!freslest_hid_map_decode(&k,1,key,8,&d));key[2]=0x1a;
 assert(freslest_hid_map_parse(&m,mouse,sizeof(mouse))&&m.kind==2);
 uint8_t move[]={5,0xff,0x9d,0x30}; // 12-bit X=-513, Y=777
 assert(freslest_hid_map_decode(&m,2,move,4,&d));assert(d.dx==-513&&d.dy==777&&d.buttons==5);
 assert(!freslest_hid_map_decode(&m,2,move,3,&d));
 // Keyboard modifiers and NKRO keys in separate numbered reports retain each other.
 uint8_t nkro[]={0x05,7,0x85,3,0x19,0xe0,0x29,0xe7,0x75,1,0x95,8,0x81,2,
  0x85,4,0x19,4,0x29,0x83,0x75,1,0x95,128,0x81,2};
 assert(freslest_hid_map_parse(&k,nkro,sizeof(nkro)));uint8_t mods=2,bitmap[16]={1,0,0,0x40};
 assert(freslest_hid_map_decode(&k,3,&mods,1,&d));assert(freslest_hid_map_decode(&k,4,bitmap,16,&d));
 assert(freslest_hid_key(d.keys,0xe1)&&freslest_hid_key(d.keys,4)&&freslest_hid_key(d.keys,34));
 memset(bitmap,0,16);assert(freslest_hid_map_decode(&k,4,bitmap,16,&d)&&freslest_hid_key(d.keys,0xe1)&&!freslest_hid_key(d.keys,4));
 // USB and BLE sources coexist; one disconnect cannot release the other's key.
 freslest_hid_state_t state={0};freslest_hid_snapshot_t snap;
 freslest_hid_state_connect(&state,0,1);freslest_hid_state_connect(&state,4,1);freslest_hid_state_connect(&state,5,2);
 freslest_hid_state_report(&state,0,key,8);freslest_hid_state_keys(&state,4,d.keys);
 freslest_hid_state_mouse(&state,5,1,INT_MAX,INT_MIN);freslest_hid_state_take(&state,&snap,true);
 assert(snap.keyboards==2&&snap.mice==1&&snap.dx==2047&&snap.dy==-2047&&snap.mouse_pressed==1);
 freslest_hid_state_disconnect(&state,4);freslest_hid_state_take(&state,&snap,true);assert(freslest_hid_key(snap.keys,0x1a));
 freslest_hid_state_disconnect(&state,5);freslest_hid_state_take(&state,&snap,true);assert(!snap.mice&&!snap.mouse_buttons&&!snap.dx);
 // Every truncated descriptor remains bounded; oversized report maps fail.
 for(size_t n=0;n<sizeof(keyboard);n++)freslest_hid_map_parse(&k,keyboard,n);
 uint8_t bad[]={0x05,7,0x75,32,0x96,0xff,0xff,0x81,2};assert(!freslest_hid_map_parse(&k,bad,sizeof(bad)));
 uint8_t pop[]={0xb4};assert(!freslest_hid_map_parse(&k,pop,sizeof(pop)));
 uint8_t longitem[]={0xfe,1,0,0};assert(!freslest_hid_map_parse(&k,longitem,sizeof(longitem)));
 puts("PASS: BLE HID descriptor/ID parsing, 6KRO/NKRO/modifiers, signed 12-bit mouse, truncation/rollover/bounds, independent report holds, USB+BLE union and disconnect release");
}
