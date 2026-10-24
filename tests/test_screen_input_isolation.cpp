// Exercise both production modules and the real HID state merger together.
#define FRESLEST_SCREEN_INPUT_INTEGRATION 1
#define FRESLEST_SCREEN_HOST_TEST 1
#define FRESLEST_USB_INPUT_HOST_TEST 1
#include "../OS/freslest/screen_mirror.h"
#include "../OS/freslest/usb_input.cpp"
#include "../OS/freslest/screen_mirror.cpp"
#include <cassert>
static freslest_hid_state_t hardware;
void freslest_hid_snapshot(freslest_hid_snapshot_t*s){freslest_hid_state_take(&hardware,s,true);}
void freslest_hid_peek(freslest_hid_snapshot_t*s){freslest_hid_state_take(&hardware,s,false);}
bool freslest_hid_start(){return true;}const char*freslest_hid_status(){return "test";}
void freslest_hid_external_connect(unsigned i,unsigned kind){freslest_hid_state_connect(&hardware,i+4,kind);}
void freslest_hid_external_disconnect(unsigned i){freslest_hid_state_disconnect(&hardware,i+4);}
void freslest_hid_external_keys(unsigned i,const uint8_t*k){freslest_hid_state_keys(&hardware,i+4,k);}
void freslest_hid_external_mouse(unsigned i,uint8_t b,int x,int y){freslest_hid_state_mouse(&hardware,i+4,b,x,y);}
void line(const char*s){Serial.rx=s;screenMirrorPoll();assert(Serial.rx.empty());}
int main(){
 usbInputBegin();line("FS1 921600 1\n");
 uint8_t up[32]{};up[0x52/8]|=1u<<(0x52%8);
 freslest_hid_external_connect(2,1);freslest_hid_external_keys(2,up);
 std::string pc="FSK 10"+std::string(62,'0')+"\n";line(pc.c_str());usbInputUpdate(200);
 assert((usbInputHeld()&5)==5); // BLE peer #2 UP and PC A/LEFT must coexist.
 assert((usbInputPressed()&5)==5);
 line(pc.c_str());usbInputUpdate(210);assert(!(usbInputPressed()&5));
 for(unsigned i=0;i<4;++i)freslest_hid_external_disconnect(i); // Actual BLE stop source range.
 line(pc.c_str());usbInputUpdate(300);assert((usbInputHeld()&4)==4);
 freslest_hid_external_connect(2,1);freslest_hid_external_keys(2,up);
 freslest_hid_external_connect(3,2);freslest_hid_external_mouse(3,1,0,0);
 line("FSP 200 60 2\n");usbInputUpdate(400);
 assert(usbPointer.visible&&usbPointer.left&&usbPointer.rightClick&&usbPointer.x==200&&usbPointer.y==60);
 line("FSR\n");usbInputUpdate(410);assert(usbInputHeld()==1&&usbPointer.visible&&usbPointer.left);
 line(pc.c_str());fakeNow+=901;screenMirrorPoll();usbInputUpdate(500);assert(usbInputHeld()==1&&usbPointer.left);
 line("FS0\n");assert(!active&&!packet);
 puts("PASS: production screen+input integration keeps PC independent of both BLE peers, BLE stop, PC release/timeout, pointer buttons and keyboard edges");
}
