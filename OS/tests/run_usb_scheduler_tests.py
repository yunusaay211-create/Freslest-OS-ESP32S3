"""Compile the shipped USB worker/event-loop functions with host RTOS doubles.

An endlessly nonempty queue previously gave the idle task no guaranteed run.
This checks the production function bodies rather than a separate scheduler.
It does not emulate USB electrical signalling or FreeRTOS on the board.
"""
from pathlib import Path
import re, subprocess, os

base=Path(__file__).resolve().parent
project=base.parent
build=base/'host_build';build.mkdir(exist_ok=True)

def function(text,signature):
    start=text.index(signature);brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]

core=project/'components/arduino/cores/esp32/esp32-hal-tinyusb.c'
device=project/'components/arduino_tinyusb/tinyusb/src/device/usbd.c'
config=(project/'components/arduino_tinyusb/include/tusb_config.h').read_text()
budget=int(re.search(r'#define\s+CFG_TUD_TASK_EVENT_BUDGET\s+(\d+)',config).group(1))
worker=function(core.read_text(),'static void usb_device_task(void *param)')
dispatch=function(device.read_text(),'void tud_task_ext(uint32_t timeout_ms, bool in_isr)')
prelude=r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#define OPT_OS_NONE 0
#define OPT_OS_PICO 1
#define CFG_TUSB_OS 2
#define CFG_TUSB_DEBUG 0
#define CFG_TUD_LOG_LEVEL 1
#define TU_LOG_USBD(...)
#define TU_LOG_BUF(...)
#define TU_ASSERT(condition,...) do { assert(condition); } while(0)
#define TU_BREAKPOINT() assert(false)
#define TUSB_DIR_OUT 0
#define TUSB_DIR_IN 1
#define TUSB_DIR_IN_MASK 128
#define SOF_CONSUMER_USER 0
enum {DCD_EVENT_BUS_RESET,DCD_EVENT_UNPLUGGED,DCD_EVENT_SETUP_RECEIVED,
 DCD_EVENT_XFER_COMPLETE,DCD_EVENT_SUSPEND,DCD_EVENT_RESUME,USBD_EVENT_FUNC_CALL,DCD_EVENT_SOF,DCD_EVENT_COUNT};
enum class xfer_result_t {SUCCESS};
struct dcd_event_t {
 unsigned event_id=USBD_EVENT_FUNC_CALL;uint8_t rhport=0;
 struct {uint8_t speed;} bus_reset{};int setup_received=0;
 struct {uint8_t ep_addr;int result;uint32_t len;} xfer_complete{};
 struct {void(*func)(void*);void* param;} func_call{};
 struct {uint32_t frame_count;} sof{};
};
struct Status{bool busy,claimed;};
struct Device {uint8_t speed=0;bool connected=false,remote_wakeup_en=false;uint32_t sof_consumer=0;Status ep_status[16][2]{};uint8_t ep2drv[16][2]{};} _usbd_dev;
unsigned _usbd_queued_setup=1;
void* _usbd_q=nullptr;
static unsigned processed=0,sleeps=0,reads=0;static bool initialized=true,emptyQueue=false;
static void callback(void*){++processed;}
bool tud_inited(){return initialized;}
bool osal_queue_receive(void*,dcd_event_t* event,uint32_t){
 ++reads;if(emptyQueue)return false;
 event->event_id=USBD_EVENT_FUNC_CALL;event->func_call={callback,nullptr};return true;
}
bool osal_queue_empty(void*){return emptyQueue;}
void usbd_reset(uint8_t){}void tud_umount_cb(){}void tud_suspend_cb(bool){}void tud_resume_cb(){}void tud_sof_cb(uint32_t){}
bool process_control_request(uint8_t,const int*){return true;}
void dcd_edpt_stall(uint8_t,uint8_t){}void usbd_control_xfer_cb(uint8_t,uint8_t,xfer_result_t,uint32_t){}
uint8_t tu_edpt_number(uint8_t n){return n&15;}uint8_t tu_edpt_dir(uint8_t n){return n>>7;}
bool tu_bit_test(uint32_t n,unsigned bit){return (n&(1u<<bit))!=0;}
struct usbd_class_driver_t {void(*xfer_cb)(uint8_t,uint8_t,xfer_result_t,uint32_t);};
void unused_transfer(uint8_t,uint8_t,xfer_result_t,uint32_t){}
usbd_class_driver_t* get_driver(uint8_t){static usbd_class_driver_t driver{unused_transfer};return &driver;}
struct Stop{};
void vTaskDelay(unsigned ticks){assert(ticks>=1);if(++sleeps==100)throw Stop{};}
void tud_task_ext(uint32_t,bool);
void tud_task(){tud_task_ext(UINT32_MAX,false);}
'''
test=r'''
int main(){
 tud_task_ext(0,false);assert(processed==CFG_TUD_TASK_EVENT_BUDGET);
 processed=0;
 try{usb_device_task(nullptr);}catch(const Stop&){}
 assert(sleeps==100&&processed==100*CFG_TUD_TASK_EVENT_BUDGET);
 emptyQueue=true;unsigned before=processed;tud_task_ext(0,false);assert(processed==before);
 initialized=false;unsigned beforeReads=reads;tud_task_ext(0,false);assert(reads==beforeReads);
 puts("PASS: actual TinyUSB worker/event-loop bodies bound an endless queue to 16 events per sleep; empty and uninitialized queues return safely");
}
'''
source=build/'usb_scheduler.cpp'
source.write_text(prelude+f'\n#define CFG_TUD_TASK_EVENT_BUDGET {budget}\n'+dispatch+'\n'+worker+'\n'+test)
binary=build/'test_usb_scheduler'
subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

# Exercise the real allocation/unwind/retry code as well. Hardware registers
# are excluded by the host target; descriptor, driver and RTOS APIs are doubled.
loader=function(core.read_text(),'static bool tinyusb_load_enabled_interfaces()')
initializer=function(core.read_text(),'esp_err_t tinyusb_init(tinyusb_device_config_t *config)')
init_prelude=r'''
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>
#define log_e(...)
#define log_d(...)
#define BIT(n) (1U<<(n))
#define TUD_CONFIG_DESC_LEN 9
#define TUD_CONFIG_DESCRIPTOR(...) 9,2,32,0,1,1,0,0,0
#define USB_INTERFACE_MAX 1
#define USB_INTERFACE_CDC 1
#define USB_INTERFACE_DFU 2
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=-2,pdPASS=1;
using tinyusb_interface_t=int;
struct tinyusb_device_config_t {};
struct tinyusb_config_t {bool external_phy;};
struct tinyusb_endpoints_usage_t {unsigned value;};
static uint16_t tinyusb_config_descriptor_len=23;
static uint8_t tinyusb_loaded_interfaces_num=0;
static uint32_t tinyusb_string_descriptor_len=4;
static tinyusb_endpoints_usage_t tinyusb_endpoints{};
static uint8_t* tinyusb_config_descriptor=nullptr;
static unsigned tinyusb_loaded_interfaces_mask=1;
static unsigned USB_DEVICE_ATTRIBUTES=0,USB_DEVICE_POWER=500;
static bool tinyusb_is_initialized=false,tinyusb_descriptors_ready=false;
static bool failMalloc=false,failLoad=false,failDriver=false,failTask=false;
static unsigned allocations=0,driverCalls=0,taskCalls=0,unwinds=0;
static void* hostMalloc(size_t n){if(failMalloc)return nullptr;++allocations;return std::malloc(n);}
static void hostFree(void* p){if(p){--allocations;std::free(p);}}
#define malloc hostMalloc
#define free hostFree
uint16_t tinyusb_load_descriptor(tinyusb_interface_t,uint8_t* dst,uint8_t* count){
 ++*count;++tinyusb_string_descriptor_len;++tinyusb_endpoints.value;
 memset(dst,0,23);return failLoad?0:23;
}
uint8_t tinyusb_add_string_descriptor(const char*){return tinyusb_string_descriptor_len++;}
void tinyusb_apply_device_config(tinyusb_device_config_t*){}
esp_err_t tinyusb_driver_install(tinyusb_config_t*){++driverCalls;return failDriver?ESP_FAIL:ESP_OK;}
void usb_device_task(void*){}
int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned bytes,void*,unsigned priority,void*,int core){
 assert(bytes==8192&&priority==5&&core==0);++taskCalls;return failTask?0:pdPASS;
}
void tud_deinit(int){++unwinds;}
void deinit_usb_hal(){++unwinds;}
'''
init_test=r'''
static void reset(){
 free(tinyusb_config_descriptor);tinyusb_config_descriptor=nullptr;
 tinyusb_config_descriptor_len=23;tinyusb_loaded_interfaces_num=0;
 tinyusb_string_descriptor_len=4;tinyusb_endpoints={};
 tinyusb_is_initialized=tinyusb_descriptors_ready=false;
 failMalloc=failLoad=failDriver=failTask=false;
 driverCalls=taskCalls=unwinds=0;assert(allocations==0);
}
int main(){
 tinyusb_device_config_t config;
 failMalloc=true;assert(tinyusb_init(&config)==ESP_FAIL);
 assert(tinyusb_config_descriptor_len==23&&!tinyusb_is_initialized&&!allocations);
 failMalloc=false;assert(tinyusb_init(&config)==ESP_OK&&allocations==1);reset();
 failLoad=true;assert(tinyusb_init(&config)==ESP_FAIL);
 assert(tinyusb_config_descriptor_len==23&&tinyusb_loaded_interfaces_num==0&&tinyusb_string_descriptor_len==4&&!tinyusb_endpoints.value&&!allocations);
 failLoad=false;assert(tinyusb_init(&config)==ESP_OK&&tinyusb_config_descriptor_len==32);reset();
 failDriver=true;assert(tinyusb_init(&config)==ESP_FAIL&&allocations==1);
 auto* descriptor=tinyusb_config_descriptor;failDriver=false;
 assert(tinyusb_init(&config)==ESP_OK&&tinyusb_config_descriptor==descriptor&&tinyusb_config_descriptor_len==32&&allocations==1);reset();
 failTask=true;assert(tinyusb_init(&config)==ESP_ERR_NO_MEM&&unwinds==2&&!tinyusb_is_initialized);
 descriptor=tinyusb_config_descriptor;failTask=false;
 assert(tinyusb_init(&config)==ESP_OK&&tinyusb_config_descriptor==descriptor&&allocations==1);
 unsigned calls=taskCalls;assert(tinyusb_init(&config)==ESP_OK&&taskCalls==calls);reset();
 puts("PASS: actual USB initialization checks worker allocation, unwinds failure, restores failed descriptor state and reuses descriptors on driver/task retry without duplicate length or leaks");
}
'''
source=build/'usb_init.cpp';source.write_text(init_prelude+loader+'\n'+initializer+'\n'+init_test)
binary=build/'test_usb_init'
subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True,timeout=5,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
