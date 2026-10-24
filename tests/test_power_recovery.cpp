#define FRESLEST_SAFETY_HOST_TEST
#include "../OS/freslest/src/freslest/power_recovery.cpp"
void resetGlobals(bool erase=true){
 if(erase)nv.clear();handles.clear();logger=defaultLogger;record={};logRing={};storeOk=false;healthyBoot=false;inRecovery=false;stable=false;stableAt=0;
 idle.begin(0);fakeNow=100;resetReason=ESP_RST_POWERON;pinLevels.clear();script.clear();scriptAt=0;onPoll={};drawn.clear();
 radioCalls=restarts=sleeps=settingsWrites=otaSelects=rtcDeinits=0;wifiEnabled=btEnabled=true;
 nvsOk=commitOk=available=correctProject=hashOk=verifyOk=storageSleepOk=true;portalActive=browserBusy=deviceRole=false;
 running={0x100000,0x1b0000,16};previousPart={0x2b0000,0x1b0000,17};previousState=ESP_OTA_IMG_VALID;
 wakeResult=btStopResult=wifiStopResult=selectResult=otaStateResult=ESP_OK;btHost=btController=0;ble={};disk={};app=APP_MENU;lastHid=0;
 for(Button*b:{&bUp,&bDown,&bLeft,&bRight,&bOk,&bBack,&bFire}){b->stable=HIGH;b->lastRead=HIGH;b->pressedEdge=false;}
}
bool saw(const char*s){return std::any_of(drawn.begin(),drawn.end(),[&](auto&v){return v.find(s)!=std::string::npos;});}
void bootTests(){
 resetGlobals();freslestSafetyBegin();assert(!freslestRecoveryRequested()&&record.policy.failures==0);
 freslestSafetyStage(FreslestBootStage::Wifi);
 for(int i=1;i<=3;++i){resetGlobals(false);resetReason=ESP_RST_EXT;freslestSafetyBegin();assert(record.policy.failures==i);assert(freslestRecoveryRequested()==(i==3));}
 assert(record.lastStage==(uint8_t)FreslestBootStage::Boot);assert(radioCalls==0);
 record.policy.resume();save();freslestSafetyTick(100,true);freslestSafetyTick(20099,true);assert(record.policy.armed);
 freslestSafetyTick(20100,true);assert(!record.policy.armed&&record.policy.failures==0);
 resetGlobals(false);resetReason=ESP_RST_SW;freslestSafetyBegin();assert(record.policy.failures==0);
 record.policy.failures=2;save();cleanExit();resetGlobals(false);resetReason=ESP_RST_SW;freslestSafetyBegin();assert(record.policy.failures==0);
 record.policy.failures=2;save();resetGlobals(false);resetReason=ESP_RST_DEEPSLEEP;freslestSafetyBegin();assert(!freslestRecoveryRequested()&&record.policy.failures==0&&rtcDeinits==2);
 resetGlobals();pinLevels[BTN_LEFT]=pinLevels[BTN_RIGHT]=LOW;freslestSafetyBegin();assert(freslestRecoveryRequested());
 resetGlobals();nvsOk=false;freslestSafetyBegin();assert(freslestRecoveryRequested());
 resetGlobals();commitOk=false;freslestSafetyBegin();assert(freslestRecoveryRequested());
 puts("PASS boot: three interrupted startups, healthy reset clearing, clean software handoff, deep-sleep exclusion/RTC release, physical recovery chord, NVS failure.");
}
void recoveryTests(){
 resetGlobals();freslestSafetyBegin();
 onPoll=[](){assert(radioCalls==0);};
 script={BTN_DOWN,BTN_DOWN,BTN_DOWN,BTN_OK,BTN_DOWN,BTN_OK,BTN_UP,BTN_UP,BTN_OK,BTN_BACK,BTN_UP,BTN_UP,BTN_OK};
 freslestRecoveryRun();assert(!wifiEnabled&&!btEnabled&&!freslestRecoveryRequested());assert(radioCalls==0&&saw("See Log"));
 assert(nv["freslest/wifi"][0]==0&&nv["freslest/bt"][0]==0);
 resetGlobals(false);freslestSafetyBegin();assert(nv["freslest/wifi"][0]==0&&nv["freslest/bt"][0]==0);
 available=false;script={BTN_DOWN,BTN_OK,BTN_UP,BTN_OK};freslestRecoveryRun();assert(!otaSelects&&saw("unavailable"));
 resetGlobals();freslestSafetyBegin();script={BTN_DOWN,BTN_DOWN,BTN_OK,BTN_BACK,BTN_UP,BTN_UP,BTN_OK};assert(errorScreen("Actual diagnostic test error"));
 assert(saw("See Log")&&saw("Last error: Actual diagnostic test error"));
 resetGlobals();freslestSafetyBegin();script={BTN_DOWN,BTN_OK,BTN_OK};
 try{freslestRecoveryRun();assert(false);}catch(Restart&){}assert(otaSelects==1&&!record.policy.recovery);
 resetGlobals();freslestSafetyBegin();commitOk=false;bool before=wifiEnabled;assert(!setRadios(false,false)&&wifiEnabled==before);
 puts("PASS recovery: actual UI toggles saved settings without any radio startup, settings survive reboot, unavailable Downdate is inert, error third action opens actual log, confirmed Downdate selects only OS, failed settings write retains old state.");
}
void validationTests(){
 resetGlobals();char version[32];assert(previousOs(version,sizeof(version))==&previousPart);
 previousState=3;assert(!previousOs(version,sizeof(version)));previousState=ESP_OTA_IMG_VALID;
 otaStateResult=ESP_ERR_NOT_FOUND;previousState=ESP_OTA_IMG_UNDEFINED;assert(!previousOs(version,sizeof(version)));
 uint8_t j[16]={};uint32_t magic=0x46524f54;memcpy(j,&magic,4);j[4]=2;j[5]=0;j[6]=j[7]=255;j[8]=0;j[9]=1;
 nv["fl_update/journal"]={j,j+16};assert(previousOs(version,sizeof(version))); // first full-installed previous slot
 j[9]=0;nv["fl_update/journal"]={j,j+16};assert(!previousOs(version,sizeof(version)));
 j[5]=3;j[6]=1;j[7]=0;nv["fl_update/journal"]={j,j+16};assert(previousOs(version,sizeof(version))); // failed trial's original source
 otaStateResult=ESP_OK;previousState=3;assert(!previousOs(version,sizeof(version))); // journal never excuses INVALID
 previousState=ESP_OTA_IMG_VALID;
 correctProject=false;assert(!previousOs(version,sizeof(version)));correctProject=true;
 hashOk=false;assert(!previousOs(version,sizeof(version)));hashOk=true;
 verifyOk=false;assert(!previousOs(version,sizeof(version)));verifyOk=true;
 previousPart.size=0xe0000;assert(!previousOs(version,sizeof(version)));previousPart.size=0x1b0000;
 running.address=0x20000;assert(!previousOs(version,sizeof(version)));
 puts("PASS Downdate: image validity, project identity, SHA, full image verification, partition size and running OS checks reject invalid images/Doom.");
}
void sleepTests(){
 resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0);
 fakeNow=299999;freslestIdleUpdate(fakeNow);assert(!sleeps);
 fakeNow=300000;try{freslestIdleUpdate(fakeNow);assert(false);}catch(Slept&){}
 assert(sleeps==1&&wakeMask==((1ULL<<BTN_OK)|(1ULL<<BTN_BACK))&&wakeMode==ESP_EXT1_WAKEUP_ANY_LOW&&!record.policy.armed);
 for(int a:{APP_USB_TRANSFER,APP_FRESLEST_UPDATE,APP_NOTE,APP_PAINT,APP_CAMERA}){
  resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0);app=a;fakeNow=300000;freslestIdleUpdate(fakeNow);assert(!sleeps&&idle.last==fakeNow);
 }
 for(int blocker=0;blocker<4;++blocker){resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0);fakeNow=300000;
  if(blocker==0)disk.state=UsbStorageState::Ready;if(blocker==1)browserBusy=true;if(blocker==2)portalActive=true;
  if(blocker==3)deviceRole=true;
  freslestIdleUpdate(fakeNow);assert(!sleeps);
 }
 resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0);fakeNow=300000;bOk.stable=LOW;freslestIdleUpdate(fakeNow);assert(!sleeps);
 bOk.stable=HIGH;fakeNow=599999;freslestIdleUpdate(fakeNow);assert(!sleeps);
 lastHid=fakeNow;fakeNow++;freslestIdleUpdate(fakeNow);assert(!sleeps);
 resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0);fakeNow=300000;wakeResult=ESP_FAIL;freslestIdleUpdate(fakeNow);assert(!sleeps&&rtcDeinits==2&&radioCalls==0);
 resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0);fakeNow=300000;storageSleepOk=false;freslestIdleUpdate(fakeNow);assert(!sleeps&&rtcDeinits==2&&radioCalls==0);
 resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0);fakeNow=300000;btHost=btController=2;ble.enabled=true;
 try{freslestIdleUpdate(fakeNow);assert(false);}catch(Slept&){}assert(btHost==0&&btController==0&&radioCalls==4);
 resetGlobals();freslestSafetyBegin();healthyBoot=true;idle.begin(0xffff0000);fakeNow=uint32_t(0xffff0000u+299999u);freslestIdleUpdate(fakeNow);assert(!sleeps);
 fakeNow++;try{freslestIdleUpdate(fakeNow);assert(false);}catch(Slept&){}
 puts("PASS sleep: exact 300000 ms inactivity, button/HID activity, USB/update/editor/portal/browser/disk blockers, RTC ANY_LOW wake, clean exit marker, failed wake setup stays awake, both radio stacks stop, millis wraparound.");
}
int main(){bootTests();recoveryTests();validationTests();sleepTests();}
