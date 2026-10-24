#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>
using esp_err_t=int;using portMUX_TYPE=int;
#define ESP_OK 0
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffffU
struct MockMutex{bool taken=false;};using SemaphoreHandle_t=MockMutex*;
struct MockQueue{size_t bytes;std::deque<std::vector<uint8_t>> commands;};using QueueHandle_t=MockQueue*;
struct MockDevice{int id;};using msc_host_device_handle_t=MockDevice*;
using msc_host_vfs_handle_t=int*;
struct msc_host_event_t{enum {MSC_DEVICE_CONNECTED,MSC_DEVICE_DISCONNECTED}event;union{uint8_t address;msc_host_device_handle_t handle;}device;};
struct msc_host_driver_config_t{bool create_backround_task;size_t task_priority,stack_size;int core_id;void(*callback)(const msc_host_event_t*,void*);void*callback_arg;};
struct esp_vfs_fat_mount_config_t{bool format_if_mount_failed;int max_files;size_t allocation_unit_size;};
struct msc_host_device_info_t{uint32_t sector_size,sector_count;};
inline bool pcRole=false,hostReady=true,busPresent=true,classInstalled=false,mountError=false,deviceError=false,removeInMount=false,removeInInfo=false,taskError=false,driverError=false,sendError=false;
inline int allocationFail=0,allocationCalls=0,queues=0,mutexes=0,volumes=0,devices=0,unregisters=0,hostStarts=0;
inline int64_t testNow=0;
inline MockDevice mockDevice{1},staleDevice{2};
inline SemaphoreHandle_t testFilesystem;
inline void(*driverCallback)(const msc_host_event_t*,void*);
inline bool usbInputDeviceMode(){return pcRole;}
inline bool freslest_hid_start(){hostStarts++;return hostReady;}
inline const char*esp_err_to_name(int){return "MOCK_ERROR";}
inline SemaphoreHandle_t xSemaphoreCreateMutex(){if(++allocationCalls==allocationFail)return nullptr;mutexes++;return testFilesystem=new MockMutex;}
inline void vSemaphoreDelete(SemaphoreHandle_t s){assert(s&&!s->taken);mutexes--;delete s;testFilesystem=nullptr;}
inline int xSemaphoreTake(SemaphoreHandle_t s,unsigned timeout){assert(s);if(s->taken){assert(timeout==0);return 0;}s->taken=true;return 1;}
inline int xSemaphoreGive(SemaphoreHandle_t s){assert(s&&s->taken);s->taken=false;return 1;}
inline QueueHandle_t xQueueCreate(int n,size_t bytes){assert(n==8);if(++allocationCalls==allocationFail)return nullptr;queues++;return new MockQueue{bytes,{}};}
inline void vQueueDelete(QueueHandle_t q){assert(q);queues--;delete q;}
inline int xQueueSend(QueueHandle_t q,const void*data,unsigned timeout){assert(q&&!timeout);if(sendError||q->commands.size()>=8)return 0;auto p=(const uint8_t*)data;q->commands.emplace_back(p,p+q->bytes);return 1;}
inline int xQueueReceive(QueueHandle_t q,void*data,unsigned){assert(q);if(q->commands.empty())return 0;memcpy(data,q->commands.front().data(),q->bytes);q->commands.pop_front();return 1;}
inline void vTaskDelay(int){}
inline int64_t esp_timer_get_time(){return testNow;}
inline int xTaskCreatePinnedToCore(void(*fn)(void*),const char*name,int stack,void*arg,int priority,void*out,int core){assert(fn&&!strcmp(name,"usb_disk")&&stack==6144&&!arg&&priority==3&&!out&&core==0);return taskError?0:1;}
inline int msc_host_install(const msc_host_driver_config_t*c){assert(c&&c->create_backround_task&&c->task_priority==4&&c->stack_size==4096&&c->core_id==0&&c->callback);if(driverError)return -1;assert(!classInstalled);classInstalled=true;driverCallback=c->callback;return 0;}
inline int msc_host_uninstall(){assert(classInstalled&&!devices&&!volumes);classInstalled=false;return 0;}
inline void notifyRemoved(msc_host_device_handle_t d){msc_host_event_t e{};e.event=msc_host_event_t::MSC_DEVICE_DISCONNECTED;e.device.handle=d;driverCallback(&e,nullptr);}
inline int msc_host_install_device(uint8_t addr,msc_host_device_handle_t*out){assert(testFilesystem->taken&&addr==7&&!devices);if(deviceError){*out=nullptr;return -1;}*out=&mockDevice;devices++;return 0;}
inline int msc_host_uninstall_device(msc_host_device_handle_t d){assert(testFilesystem->taken&&d==&mockDevice&&devices==1&&!volumes);devices--;return 0;}
inline int msc_host_vfs_register(msc_host_device_handle_t d,const char*root,const esp_vfs_fat_mount_config_t*c,msc_host_vfs_handle_t*out){assert(testFilesystem->taken&&d==&mockDevice&&!strcmp(root,"/usb")&&!c->format_if_mount_failed&&c->max_files==3);if(mountError){*out=nullptr;return -1;}*out=new int(1);volumes++;if(removeInMount){removeInMount=false;notifyRemoved(d);busPresent=false;}return 0;}
inline int msc_host_vfs_unregister(msc_host_vfs_handle_t v){assert(testFilesystem->taken&&v&&volumes==1);delete v;volumes--;unregisters++;return 0;}
inline int msc_host_vfs_get_last_mount_result(){return 13;}
inline int msc_host_get_device_info(msc_host_device_handle_t d,msc_host_device_info_t*out){assert(testFilesystem->taken&&d==&mockDevice);out->sector_size=512;out->sector_count=65536;if(removeInInfo){removeInInfo=false;notifyRemoved(d);busPresent=false;}return 0;}
inline int usb_host_device_addr_list_fill(int n,uint8_t*list,int*count){assert(n>=1);*count=busPresent?1:0;if(busPresent)list[0]=7;return 0;}

inline bool usbInputHostMode(){return !pcRole;}
