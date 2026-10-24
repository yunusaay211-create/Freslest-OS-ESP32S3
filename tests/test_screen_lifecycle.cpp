#define FRESLEST_SCREEN_HOST_TEST
#include "../OS/freslest/screen_mirror.cpp"
#include <cassert>
#include <vector>
void commandLine(const char*s){Serial.rx=s;screenMirrorPoll();assert(Serial.rx.empty());}
int main(){
 commandLine("FS?\n");assert(Serial.tx.find("FSHELLO 2 UART 480 320")!=std::string::npos);
 commandLine("FS1 9600 1\n");assert(!screenMirrorActive()&&!writer);
 mallocFail=true;commandLine("FS1 921600 1\n");assert(!active&&!packet&&Serial.tx.find("FSERROR PSRAM")!=std::string::npos);
 mallocFail=false;taskFail=true;commandLine("FS1 921600 1\n");assert(!active&&!packet&&!writer);
 taskFail=false;commandLine("FS1 921600 1\n");assert(active&&writer&&packet&&Serial.baud==921600);assert(allocationCaps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 commandLine("FS2\n");assert(wanted);
 std::vector<uint16_t> raw(480*320,0xffff);screenMirrorFrame(raw.data(),480,320,true,5,6);
 assert(busy.load()&&!wanted&&notifications==1&&packetLength&&memcmp(packet,"FSCRN1\r\n",8)==0);
 auto* saved=packet;auto n=packetLength;std::vector<uint8_t> copy(packet,packet+n);
 raw[0]=0;commandLine("FS2\n");screenMirrorFrame(raw.data(),480,320,false,0,0);
 assert(notifications==1&&n==packetLength&&!memcmp(packet,copy.data(),n));
 commandLine("FS0\n");assert(stopping&&packet==saved&&Serial.baud==921600);
 busy.store(false);screenMirrorPoll();assert(!active&&!packet&&Serial.baud==115200);
 commandLine("mi\n");assert(screenMirrorDiagnostics()==3&&!screenMirrorDiagnostics());
 commandLine("FS1 460800 2\n");assert(active&&divisor==2&&Serial.baud==460800);
 fakeNow+=5001;screenMirrorPoll();assert(!active&&!packet&&Serial.baud==115200);
 std::string longline="F"+std::string(120,'x')+"\n";commandLine(longline.c_str());assert(!used[0]&&!overflow[0]);
 fakeNow=UINT32_MAX-2000;commandLine("FS1 921600 1\n");fakeNow+=5001;screenMirrorPoll();assert(!active&&!packet);
 // Remote input shares the real HID merger without releasing BLE input.
 commandLine("FS1 921600 1\n");
 uint8_t ble[32]{};ble[0xe0/8]|=1u<<(0xe0%8);freslest_hid_external_connect(0,1);freslest_hid_external_keys(0,ble);
 freslest_hid_snapshot_t snap{};freslest_hid_state_take(&remoteState,&snap,true);
 std::string keys="FSK "+std::string(64,'0')+"\n";keys[4]='1';commandLine(keys.c_str());
 freslest_hid_state_take(&remoteState,&snap,true);assert(freslest_hid_key(snap.keys,4)&&freslest_hid_key(snap.pressed,4));
 commandLine(keys.c_str());freslest_hid_state_take(&remoteState,&snap,true);assert(!freslest_hid_key(snap.pressed,4));
 commandLine("FSP 100 50 1\nFSP 200 60 0\n");freslest_hid_state_take(&remoteState,&snap,true);
 assert(usbPointer.x==200&&usbPointer.y==60&&snap.dx==0&&snap.dy==0&&snap.mouse_pressed==1&&!snap.mouse_buttons);
 commandLine("FSP -1 20 1\nFSP 480 20 1\n");assert(usbPointer.x==200);
 fakeNow+=901;screenMirrorPoll();freslest_hid_state_take(&remoteState,&snap,true);
 assert(!freslest_hid_key(snap.keys,4)&&freslest_hid_key(snap.keys,0xe0)&&!snap.mice);
 commandLine("FS0\n");assert(!active);
 assert(screenMirrorNativeBegin());Native.rx="FS?\n";screenMirrorPoll();assert(Native.tx.find("FSHELLO 2 NATIVE 480 320")!=std::string::npos);
 Native.rx="FS1 921600 2\n";screenMirrorPoll();assert(active&&owner==1&&Serial.baud==115200);
 commandLine("FS0\nFS1 460800 1\n");assert(active&&!stopping&&Serial.tx.find("FSBUSY")!=std::string::npos);
 Native.rx="FS2\n";screenMirrorPoll();assert(wanted);screenMirrorFrame(raw.data(),480,320,false,0,0);assert(busy.load());
 nativeConnected=false;screenMirrorPoll();assert(stopping&&packet);busy.store(false);screenMirrorPoll();assert(!active&&!packet&&Serial.baud==115200);
 puts("PASS: UART/native signature, ownership, PSRAM-only transfer, immutable packet, HID edges/absolute pointer, BLE isolation, input/stream timeout and disconnect cleanup");
}
