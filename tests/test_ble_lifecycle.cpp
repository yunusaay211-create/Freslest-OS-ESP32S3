#define FRESLEST_BLE_HOST_TEST
#include "../OS/freslest/src/freslest/ble_advertising.cpp"
extern "C" {
#include <freslest_hid_state.h>
}
static freslest_hid_state_t state{};
void freslest_hid_external_connect(unsigned i,unsigned kind){assert(i<4);freslest_hid_state_connect(&state,i+4,kind);}
void freslest_hid_external_disconnect(unsigned i){assert(i<4);freslest_hid_state_disconnect(&state,i+4);}
void freslest_hid_external_keys(unsigned i,const uint8_t*k){freslest_hid_state_keys(&state,i+4,k);}
void freslest_hid_external_mouse(unsigned i,uint8_t b,int x,int y){freslest_hid_state_mouse(&state,i+4,b,x,y);}
static void pump(){Command c;while(commands&&xQueueReceive(commands,&c,0))process(c);}
static void openEvent(esp_hidh_dev_t&d){esp_hidh_event_data_t e{};e.open={&d,0};hid(nullptr,nullptr,ESP_HIDH_OPEN_EVENT,&e);}
static void closeEvent(esp_hidh_dev_t&d){esp_hidh_event_data_t e{};e.close={&d,0x13};hid(nullptr,nullptr,ESP_HIDH_CLOSE_EVENT,&e);d.freed=true;}
static void inputEvent(esp_hidh_dev_t&d,unsigned id,uint8_t*b,unsigned n){esp_hidh_event_data_t e{};e.input={&d,0,id,n,b};hid(nullptr,nullptr,ESP_HIDH_INPUT_EVENT,&e);}
static void advertise(esp_hidh_dev_t&d,bool uuid=true){esp_ble_gap_cb_param_t p{};p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_RES_EVT;memcpy(p.scan_rst.bda,d.address,6);p.scan_rst.ble_addr_type=1;if(uuid){uint8_t ad[]={3,3,0x12,0x18};memcpy(p.scan_rst.ble_adv,ad,4);p.scan_rst.adv_data_len=4;}gap(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);}
int main(){
 const uint8_t kd[]={0x05,7,0x85,1,0x19,0xe0,0x29,0xe7,0x75,1,0x95,8,0x81,2,0x75,8,0x95,1,0x81,1,0x19,0,0x29,0x65,0x75,8,0x95,6,0x81,0};
 const uint8_t md[]={0x05,9,0x85,2,0x19,1,0x29,3,0x75,1,0x95,3,0x81,2,0x75,5,0x95,1,0x81,1,0x05,1,0x09,0x30,0x09,0x31,0x15,0x81,0x75,8,0x95,2,0x81,6};
 esp_hidh_dev_t k{{1,2,3,4,5,6},{{kd,sizeof(kd)},{}},1},m{{6,5,4,3,2,1},{{md,sizeof(md)},{}},1};
 internalFree=64000;freslestBleStart();pump();assert(!info.enabled&&!info.ready&&!sdkStarts);
 internalFree=131072;internalLargest=8192;freslestBleStart();pump();assert(!info.enabled&&!info.ready&&!sdkStarts);
 internalLargest=65536;bluedroidState=ESP_BLUEDROID_STATUS_UNINITIALIZED;freeAfterInit=16000;
 freslestBleStart();pump();assert(!info.enabled&&!info.ready&&!sdkStarts&&!bluedroidEnables);
 internalFree=freeAfterInit=131072;freslestBleStart();pump();assert(info.enabled&&info.ready&&scans==1&&sdkStarts==1&&bluedroidEnables==1);
 advertise(k);advertise(m);openHook=[&](uint8_t*b,int){auto*d=same(b,k.address)?&k:&m;d->freed=false;openEvent(*d);return d;};
 freslestBleConnect(0);pump();freslestBleConnect(1);pump();assert(info.connected==2&&saved.count==2&&!info.busy);
 persist(fakeNow);assert(!nvsBytes.empty());
 uint8_t keys[]={0,0,0x1a,0,0,0,0,0},mouse[]={1,5,0xfb};inputEvent(k,1,keys,8);inputEvent(m,2,mouse,3);
 freslest_hid_snapshot_t snap;freslest_hid_state_take(&state,&snap,true);assert(snap.keyboards==1&&snap.mice==1&&snap.dx==5&&snap.dy==-5&&snap.mouse_buttons==1);
 inputEvent(m,2,mouse,2);freslest_hid_state_take(&state,&snap,true);assert(!snap.dx&&!snap.dy);
 freslest_hid_state_connect(&state,0,1);freslest_hid_state_report(&state,0,keys,8);
 closeEvent(k);freslest_hid_state_take(&state,&snap,true);assert(snap.keyboards==1&&freslest_hid_key(snap.keys,0x1a)&&snap.mice==1);
 closeEvent(m);freslest_hid_state_take(&state,&snap,true);assert(!snap.mice&&!snap.mouse_buttons);
 unsigned before=opens;fakeNow+=1000;advertise(m,false);maintenance(fakeNow);assert(opens==before+1&&info.connected==1);
 freslestBleConnect(1);pump();closeEvent(m);before=opens;fakeNow+=7000;advertise(m,false);maintenance(fakeNow);assert(opens==before);
 freslestBleStop();pump();assert(!info.enabled&&!info.connected);
 saved={0x46424c45,1,0,{}};memset(retry,0,sizeof(retry));loaded=initialized=privacyReady=false;info={};
 freslestBleStart();pump();assert(saved.count==2&&info.count==2&&info.devices[1].remembered);
 before=opens;maintenance(fakeNow);assert(opens==before);fakeNow+=50;advertise(m,false);maintenance(fakeNow);assert(opens==before+1&&info.connected==1);
 closeEvent(m);fakeNow+=1000;openHook=[&](uint8_t*,int){freslestBleStop();m.freed=false;openEvent(m);m.freed=true;return &m;};
 advertise(m,false);maintenance(fakeNow);pump();assert(!info.enabled&&!info.connected&&rejectCount);maintenance(fakeNow);assert(!rejectCount);
 openHook=[](uint8_t*,int)->esp_hidh_dev_t*{return nullptr;};freslestBleStart();pump();fakeNow+=100;advertise(m,false);maintenance(fakeNow);before=opens;
 maintenance(fakeNow+100);assert(opens==before);fakeNow+=2100;advertise(m,false);maintenance(fakeNow);assert(opens==before+1);
 nvsFail=true;dirty=true;saveAfter=0;persist(fakeNow);assert(dirty&&saveAfter==fakeNow+5000);nvsFail=false;persist(fakeNow+5000);assert(!dirty);
 assert(due(20,UINT32_MAX-5)&&!due(UINT32_MAX-20,30));
 freslestBleStop();pump();assert(!critical);
 puts("PASS: complete BLE production source: persistent peers, reboot/wake reconnect without UUID, two HID devices, real report parser/state, USB key retention, unplug release, manual pause, stale SDK OPEN cancellation, retry backoff, wrap-safe timers and NVS retry.");
}
