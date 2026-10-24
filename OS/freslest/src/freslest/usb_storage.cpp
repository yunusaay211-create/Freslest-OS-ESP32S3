#include "usb_storage.h"
#include <cstdio>
#include <cstring>
#ifdef FRESLEST_USB_STORAGE_HOST_TEST
#include "usb_storage_host_platform.h"
#else
#include "../../usb_input.h"
#include <freslest_hid.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <usb/msc_host.h>
#include <usb/msc_host_vfs.h>
#include <usb/usb_host.h>
#endif
namespace {
enum CommandType { Connect, Remove, Retry, Eject };
struct Command { CommandType type; uint8_t address; msc_host_device_handle_t handle; };
QueueHandle_t events;
SemaphoreHandle_t filesystem;
msc_host_device_handle_t device;
msc_host_vfs_handle_t volume;
uint8_t address;
bool started, sleeping;
UsbStorageInfo info;
portMUX_TYPE stateLock=portMUX_INITIALIZER_UNLOCKED;
msc_host_device_handle_t visibleDevice;
uint32_t disconnects;
void setInfo(UsbStorageState state,const char *message,uint64_t bytes){
  info.state=state;info.bytes=bytes;++info.revision;
  snprintf(info.message,sizeof(info.message),"%s",message);
}
void publish(UsbStorageState state,const char *message,uint64_t bytes=0){
  portENTER_CRITICAL(&stateLock);setInfo(state,message,bytes);
  portEXIT_CRITICAL(&stateLock);
}
bool gone(uint32_t token){
  portENTER_CRITICAL(&stateLock);bool changed=disconnects!=token;portEXIT_CRITICAL(&stateLock);return changed;
}
void unmount(){
  portENTER_CRITICAL(&stateLock);visibleDevice=nullptr;portEXIT_CRITICAL(&stateLock);
  if(volume){msc_host_vfs_unregister(volume);volume=nullptr;}
  if(device){msc_host_uninstall_device(device);device=nullptr;}
}
void callback(const msc_host_event_t *event,void*){
  Command cmd{};
  if(event->event==msc_host_event_t::MSC_DEVICE_CONNECTED){cmd.type=Connect;cmd.address=event->device.address;}
  else if(event->event==msc_host_event_t::MSC_DEVICE_DISCONNECTED){
    cmd.type=Remove;cmd.handle=event->device.handle;
    portENTER_CRITICAL(&stateLock);
    if(!visibleDevice||visibleDevice==cmd.handle){++disconnects;setInfo(UsbStorageState::Absent,"USB drive removed",0);}
    portEXIT_CRITICAL(&stateLock);
  }else return;
  // No filesystem or device cleanup in the class-driver callback.
  if(xQueueSend(events,&cmd,0)!=pdTRUE)
    publish(UsbStorageState::Error,"USB event queue full. Reconnect the drive.");
}
void mount(){
  portENTER_CRITICAL(&stateLock);uint32_t token=disconnects;portEXIT_CRITICAL(&stateLock);
  publish(UsbStorageState::Mounting,"Reading USB drive...");
  esp_err_t err=ESP_OK;
  if(!device)err=msc_host_install_device(address,&device);
  portENTER_CRITICAL(&stateLock);visibleDevice=device;portEXIT_CRITICAL(&stateLock);
  if(gone(token)){unmount();return;}
  if(err!=ESP_OK){
    char text[80];snprintf(text,sizeof(text),"USB device: %s. OK retries.",esp_err_to_name(err));
    publish(UsbStorageState::Error,text);return;
  }
  esp_vfs_fat_mount_config_t cfg{};
  cfg.format_if_mount_failed=false;cfg.max_files=3;cfg.allocation_unit_size=0;
  err=msc_host_vfs_register(device,usbStorageRoot(),&cfg,&volume);
  if(gone(token)){unmount();return;}
  if(err!=ESP_OK||!volume){
    char text[80];snprintf(text,sizeof(text),"FAT16/FAT32 mount failed (FR=%d). OK retries.",msc_host_vfs_get_last_mount_result());
    publish(UsbStorageState::Error,text);return;
  }
  msc_host_device_info_t desc{};uint64_t bytes=0;
  if(msc_host_get_device_info(device,&desc)==ESP_OK)bytes=(uint64_t)desc.sector_size*desc.sector_count;
  // Publish under the same lock as removal: a late disconnect cannot be
  // overwritten by READY, even if it arrives during the device-info request.
  portENTER_CRITICAL(&stateLock);
  if(disconnects==token)setInfo(UsbStorageState::Ready,"FAT USB drive ready",bytes);
  portEXIT_CRITICAL(&stateLock);
  if(gone(token))unmount();
}
void process(const Command &cmd){
  if(cmd.type==Connect&&cmd.address==0)return;
  // Lock precedes all VFS/MSC lifetime changes. UI locks use a zero timeout.
  xSemaphoreTake(filesystem,portMAX_DELAY);
  if(sleeping){xSemaphoreGive(filesystem);return;}
  if(cmd.type==Connect){
    unmount();address=cmd.address;mount();
  }else if(cmd.type==Remove){
    if(!cmd.handle||cmd.handle==device){unmount();address=0;publish(UsbStorageState::Absent,"No USB drive connected");}
  }else if(cmd.type==Eject){
    publish(UsbStorageState::Mounting,"Removing USB drive...");unmount();
    publish(address?UsbStorageState::Ejected:UsbStorageState::Absent,address?"Safe to remove. OK reconnects.":"No USB drive connected");
  }else if(address){
    unmount();mount();
  }
  xSemaphoreGive(filesystem);
}
void checkPresence(){
  if(!address)return;
  uint8_t list[16];int count=0;
  if(usb_host_device_addr_list_fill(sizeof(list),list,&count)!=ESP_OK)return;
  for(int i=0;i<count;i++)if(list[i]==address)return;
  process({Remove,0,nullptr});
}
void worker(void*){
  int64_t lastCheck=0;
  for(;;){
    Command cmd{};
    if(xQueueReceive(events,&cmd,pdMS_TO_TICKS(100))==pdTRUE)process(cmd);
    int64_t now=esp_timer_get_time();
    if(now-lastCheck>=500000){lastCheck=now;checkPresence();}
    vTaskDelay(1);
  }
}
bool request(CommandType type){
  if(!started)return false;
  Command cmd{type,0,nullptr};return xQueueSend(events,&cmd,0)==pdTRUE;
}
}
bool usbStorageBegin(){
  if(started)return true;
  if(!usbInputHostMode()){publish(UsbStorageState::Disabled,"USB device mode. Switch to USB host to use a drive.");return false;}
  if(!freslest_hid_start()){publish(UsbStorageState::Disabled,"USB host unavailable. Reopen after a restart.");return false;}
  filesystem=xSemaphoreCreateMutex();events=xQueueCreate(8,sizeof(Command));
  if(!filesystem||!events)goto fail;
  publish(UsbStorageState::Absent,"Connect a FAT16/FAT32 USB drive to USB/OTG.");
  {
    msc_host_driver_config_t cfg{};cfg.create_backround_task=true;cfg.task_priority=4;
    cfg.stack_size=4096;cfg.core_id=0;cfg.callback=callback;
    if(msc_host_install(&cfg)!=ESP_OK)goto fail;
    if(xTaskCreatePinnedToCore(worker,"usb_disk",6144,nullptr,3,nullptr,0)!=pdPASS){msc_host_uninstall();goto fail;}
  }
  started=true;return true;
fail:
  if(events){vQueueDelete(events);events=nullptr;}
  if(filesystem){vSemaphoreDelete(filesystem);filesystem=nullptr;}
  publish(UsbStorageState::Disabled,"USB storage unavailable. Restart and try again.");return false;
}
UsbStorageInfo usbStorageInfo(){
  portENTER_CRITICAL(&stateLock);UsbStorageInfo snapshot=info;portEXIT_CRITICAL(&stateLock);return snapshot;
}
const char *usbStorageRoot(){return "/usb";}
bool usbStorageRetry(){return request(Retry);}
bool usbStorageEject(){return request(Eject);}
bool usbStorageLock(){
  if(!filesystem||usbStorageInfo().state!=UsbStorageState::Ready||xSemaphoreTake(filesystem,0)!=pdTRUE)return false;
  if(usbStorageInfo().state==UsbStorageState::Ready)return true;
  xSemaphoreGive(filesystem);return false;
}
void usbStorageUnlock(){xSemaphoreGive(filesystem);}
bool usbStoragePrepareSleep(){
  if(!started)return true;
  if(xSemaphoreTake(filesystem,0)!=pdTRUE)return false;
  bool safe=!device&&!volume&&!address&&!usbStorageInfo().present();
  if(safe)sleeping=true;
  xSemaphoreGive(filesystem);return safe;
}
