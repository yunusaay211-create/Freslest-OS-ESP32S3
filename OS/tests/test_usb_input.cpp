#define FRESLEST_USB_INPUT_HOST_TEST 1
#include "../freslest/usb_input.cpp"
#include "freslest_hid_state.h"
#include <cassert>
static freslest_hid_state_t decoder;
void freslest_hid_snapshot(freslest_hid_snapshot_t* out){freslest_hid_state_take(&decoder,out,true);}void freslest_hid_peek(freslest_hid_snapshot_t* out){freslest_hid_state_take(&decoder,out,false);}bool freslest_hid_start(){return true;}const char* freslest_hid_status(){return "test";}
int main(){
 usbInputBegin();assert(!usbInputDeviceMode());freslest_hid_state_connect(&decoder,0,1);freslest_hid_state_connect(&decoder,1,2);
 uint8_t w[]={0,0,0x1a,0,0,0,0,0},up[8]={0};freslest_hid_state_report(&decoder,0,w,8);freslest_hid_state_report(&decoder,0,up,8);usbInputUpdate(100);assert(usbInputLastActivity()==100);assert((usbInputPressed()&1)&&!usbInputHeld()&&(usbInputNesPad()&16));assert(!usbInputNesPad());
 freslest_hid_state_report(&decoder,0,w,8);usbInputUpdate(200);assert(usbInputHeld()&1);usbInputUpdate(400);assert(!usbInputPressed());usbInputUpdate(550);assert(usbInputPressed()&1);
 uint8_t pad[]={0x20,0,0x1b,0x1d,0x28,0x4f,0,0};freslest_hid_state_report(&decoder,0,pad,8);usbInputUpdate(600);assert((usbInputNesPad()&0x8f)==0x8f);
 usbInputTextMode(true);freslest_hid_state_report(&decoder,0,up,8);usbInputUpdate(700);uint8_t a[]={2,0,4,0,0,0,0,0};freslest_hid_state_report(&decoder,0,a,8);usbInputUpdate(800);assert(usbInputCharacter()=='A'&&!usbInputHeld());usbInputUpdate(1200);assert(usbInputLastActivity()==1200);assert(usbInputCharacter()=='A');usbInputTextMode(false);
 uint8_t mouse[]={1,127,127};for(int i=0;i<20;i++)freslest_hid_state_report(&decoder,1,mouse,3);usbInputUpdate(1300);assert(usbPointer.visible&&usbPointer.click&&usbPointer.left&&usbPointer.x==479&&usbPointer.y==319);assert(usbInputHit(470,310,10,10));freslest_hid_state_disconnect(&decoder,0);freslest_hid_state_disconnect(&decoder,1);usbInputUpdate(1400);assert(usbInputLastActivity()==1300);assert(!usbPointer.visible&&!usbInputHeld());
 tft.heightValue=360;freslest_hid_state_connect(&decoder,5,2);
 freslest_hid_state_mouse(&decoder,5,1,2000,2000);usbInputUpdate(1500);
 assert(usbPointer.x==479&&usbPointer.y==359&&usbInputHit(330,336,150,24));
 freslest_hid_state_disconnect(&decoder,5);usbInputUpdate(1600);tft.heightValue=320;
 // Browser reserves Tab for the page and right-click for its context menu.
 app=APP_BROWSER;usbInputTextMode(true);freslest_hid_state_connect(&decoder,0,1);freslest_hid_state_connect(&decoder,1,2);
 uint8_t tab[]={0,0,0x2b,0,0,0,0,0},rightMouse[]={2,0,0};freslest_hid_state_report(&decoder,0,tab,8);freslest_hid_state_report(&decoder,1,rightMouse,3);usbInputUpdate(1700);
 assert(!usbInputTasks()&&!strcmp(usbInputSpecialKey(),"Tab")&&usbPointer.rightClick&&!(usbInputPressed()&32));
 freslest_hid_state_report(&decoder,0,up,8);usbInputUpdate(1710);uint8_t ctrlA[]={1,0,4,0,0,0,0,0};freslest_hid_state_report(&decoder,0,ctrlA,8);usbInputUpdate(1720);assert(!strcmp(usbInputSpecialKey(),"Control+a")&&usbInputCharacter()==-1);
 app=0;usbInputTextMode(false);freslest_hid_state_disconnect(&decoder,0);freslest_hid_state_disconnect(&decoder,1);usbInputUpdate(1730);assert(usbInputLastActivity()==1720);
 freslest_hid_state_connect(&decoder,1,2);usbInputUpdate(3000);assert(usbPointer.visible&&usbInputLastActivity()==1720);
 freslest_hid_state_disconnect(&decoder,1);usbInputUpdate(3010);
 puts("PASS: Browser keyboard Tab/Ctrl+A forwarding and right-click preserve page input without opening Task Switcher or BACK");
 nvsFail=true;assert(!usbInputSwitchMode(true));nvsFail=false;try{usbInputSwitchMode(true);assert(false);}catch(const Restart&){}assert(savedPc==1&&!savedNes);usbInputBegin();assert(usbInputDeviceMode());try{usbInputSwitchMode(false);assert(false);}catch(const Restart&){}usbInputBegin();assert(!usbInputDeviceMode()&&usbInputResumeNes()&&!savedNes);
 nvsFail=true;assert(!usbInputSwitchViewer(true));nvsFail=false;
 try{usbInputSwitchViewer(true);assert(false);}catch(const Restart&){}
 assert(savedScreen==1&&!savedPc&&!savedNes);usbInputBegin();assert(usbInputViewerMode()&&!usbInputDeviceMode()&&!usbInputHostMode());
 try{usbInputSwitchMode(false);assert(false);}catch(const Restart&){}
 usbInputBegin();assert(usbInputHostMode()&&!usbInputResumeNes());
 try{usbInputSwitchViewer(true);assert(false);}catch(const Restart&){}
 usbInputBegin();assert(usbInputViewerMode());
 try{usbInputSwitchMode(true);assert(false);}catch(const Restart&){}
 usbInputBegin();assert(usbInputDeviceMode()&&!usbInputViewerMode()&&!savedScreen);
 try{usbInputSwitchViewer(false);assert(false);}catch(const Restart&){}
 usbInputBegin();assert(usbInputHostMode()&&!savedPc&&!savedNes&&!savedScreen);
 puts("PASS: actual OS USB navigation, tap latches/repeat, NES controls, typing, pointer clamp/hit/unplug and persisted USB roles with save-error protection");
}
