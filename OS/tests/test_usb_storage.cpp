#define FRESLEST_USB_STORAGE_HOST_TEST 1
#include "../freslest/src/freslest/usb_storage.cpp"
#include <cstdio>
static void drain(){Command cmd{};while(xQueueReceive(events,&cmd,0)==pdTRUE)process(cmd);}
static void reset(){
 if(filesystem&&device)process({Remove,0,nullptr});
 assert(!volumes&&!devices);
 if(classInstalled)msc_host_uninstall();
 if(events)vQueueDelete(events);if(filesystem)vSemaphoreDelete(filesystem);
 events=nullptr;filesystem=nullptr;volume=nullptr;device=visibleDevice=nullptr;address=0;started=false;sleeping=false;disconnects=0;info={};
 pcRole=false;hostReady=busPresent=true;mountError=deviceError=removeInMount=removeInInfo=taskError=driverError=sendError=false;allocationFail=allocationCalls=hostStarts=unregisters=0;
 assert(!queues&&!mutexes);
}
static void connect(){msc_host_event_t e{};e.event=msc_host_event_t::MSC_DEVICE_CONNECTED;e.device.address=7;callback(&e,nullptr);drain();}
int main(){
 reset();pcRole=true;assert(!usbStorageBegin()&&!hostStarts&&!queues&&!mutexes);
 reset();hostReady=false;assert(!usbStorageBegin()&&!queues&&!mutexes);
 for(int stage=1;stage<=4;stage++){
  reset();if(stage<=2)allocationFail=stage;else if(stage==3)driverError=true;else taskError=true;
  assert(!usbStorageBegin()&&!classInstalled&&!queues&&!mutexes&&hostReady);
 }
 reset();assert(usbStorageBegin()&&classInstalled&&hostStarts==1);assert(usbStorageBegin()&&hostStarts==1);
 connect();assert(usbStorageInfo().state==UsbStorageState::Ready&&usbStorageInfo().bytes==33554432&&volumes==1&&devices==1);
 assert(usbStorageLock());int before=unregisters;notifyRemoved(&mockDevice);
 assert(usbStorageInfo().state==UsbStorageState::Absent&&!usbStorageLock()&&volumes==1&&unregisters==before);
 usbStorageUnlock();drain();assert(!devices&&!volumes&&!address);
 busPresent=true;connect();notifyRemoved(&staleDevice);drain();assert(usbStorageInfo().state==UsbStorageState::Ready&&volumes==1);
 assert(usbStorageEject());drain();assert(usbStorageInfo().state==UsbStorageState::Ejected&&!volumes&&!devices&&!usbStorageLock());
 assert(usbStorageRetry());drain();assert(usbStorageInfo().state==UsbStorageState::Ready&&volumes==1);
 busPresent=false;checkPresence();assert(usbStorageInfo().state==UsbStorageState::Absent&&!devices&&!volumes);
 busPresent=true;mountError=true;connect();assert(usbStorageInfo().state==UsbStorageState::Error&&devices==1&&!volumes&&!usbStorageLock());
 mountError=false;assert(usbStorageRetry());drain();assert(usbStorageInfo().state==UsbStorageState::Ready);
 reset();assert(usbStorageBegin());removeInMount=true;connect();assert(usbStorageInfo().state==UsbStorageState::Absent&&!volumes&&!devices);
 reset();assert(usbStorageBegin());removeInInfo=true;connect();assert(usbStorageInfo().state==UsbStorageState::Absent&&!volumes&&!devices);
 reset();assert(usbStorageBegin());deviceError=true;connect();assert(usbStorageInfo().state==UsbStorageState::Error&&!devices);busPresent=false;checkPresence();assert(usbStorageInfo().state==UsbStorageState::Absent);
 reset();assert(usbStoragePrepareSleep());assert(usbStorageBegin());
 xSemaphoreTake(filesystem,0);assert(!usbStoragePrepareSleep());xSemaphoreGive(filesystem);
 connect();assert(!usbStoragePrepareSleep());notifyRemoved(&mockDevice);assert(!usbStoragePrepareSleep());drain();
 assert(usbStoragePrepareSleep());connect();assert(!device&&!volume&&!address); // a queued late connect cannot mount after the sleep gate
 reset();
 puts("PASS: real USB storage service shares HID host; no format; allocation/driver/task failures clean up; mount/retry/eject/replug; callback defers unmount until readers unlock; stale removal ignored; removal during mount/info cannot resurrect D:; failed-device disappearance polled; sleep gate refuses live/busy media and prevents late mounts");
}
