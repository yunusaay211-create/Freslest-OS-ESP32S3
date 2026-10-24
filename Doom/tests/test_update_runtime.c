#define FRESLEST_OTA_HOST_TEST 1
#include "freslest_update_package.h"
#include "../../shared_components/freslest_ota/freslest_ota.c"
#define FRESLEST_UPDATE_RUNTIME_TEST 1
#define USB_ROOT "usb"
#include "../../common/usb_update/freslest_update.c"
uint8_t mock_mode,mock_saved[32],mock_staged[32];size_t mock_saved_size,mock_staged_size;
bool mock_nvs_fail,mock_heap_fail,mock_bad_running,mock_boot_fail,mock_read_corrupt,mock_end_fail,mock_begin_fail;
unsigned mock_allocations,mock_erases,mock_writes,mock_unmounts,mock_first_erase_mode,mock_running,mock_events,mock_cut_at,mock_ota_states[3];
int mock_write_failure=-1;int64_t mock_time;uint8_t *mock_flash;void *mock_heap_block;
esp_partition_t mock_parts[]={
 {0x100000,0x1b0000,0,16,"os_a"},{0x2b0000,0x1b0000,0,17,"os_b"},{0x20000,0xe0000,0,18,"doom"},
 {0x460000,0x950000,0x42,6,"wad"},{0xdb0000,0x50000,1,0x82,"spiffs"},{0xe00000,0x200000,1,0x40,"saves"}};
const esp_partition_t *mock_boot;jmp_buf mock_restart;
static uint8_t *original;static size_t original_size;static freslest_update_package valid_package;
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p) {
 char error[100];if(mock_boot_fail)return ESP_FAIL;
 if(p==&mock_os&&!freslest_update_validate_image(mock_flash,original_size,&valid_package,error,sizeof(error)))return ESP_FAIL;
 mock_event();mock_boot=p;mock_ota_states[p->subtype-16]=ESP_OTA_IMG_NEW;mock_event();return ESP_OK;
}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void) {mock_ota_states[mock_running]=ESP_OTA_IMG_INVALID;mock_boot=&mock_parts[mock_running^1];esp_restart();}
void freslest_update_draw_indexed(const uint8_t *p) {(void)p;if(last_percent==100)assert(mock_boot==&mock_os);}
static void policy_reset(void) {memset(&state,0,sizeof(state));begun=recovered=false;pending=ready=false;frames=0;started=0;deadline=NULL;}
static void candidate_from(const char *name) {
 char path[160];snprintf(path,sizeof(path),"usb/%s",name);FILE *f=fopen(path,"rb");assert(f);
 assert(freslest_update_inspect(f,&packages[0].package,error_text,sizeof(error_text)));fclose(f);
 snprintf(packages[0].filename,sizeof(packages[0].filename),"%s",name);count=1;selected=0;mounted=drive=(void*)1;
}
static void reset(const char *name) {
 if(mock_heap_block)free(mock_heap_block);mock_heap_block=NULL;
 memset(mock_flash,0x3c,FRESLEST_OS_SLOT_SIZE);mock_mode=0;mock_saved_size=mock_staged_size=0;mock_boot=&mock_parts[0];mock_running=0;mock_time=0;
 mock_nvs_fail=mock_heap_fail=mock_bad_running=mock_boot_fail=mock_read_corrupt=mock_end_fail=mock_begin_fail=false;
 mock_allocations=mock_erases=mock_writes=mock_unmounts=mock_first_erase_mode=mock_events=mock_cut_at=0;mock_write_failure=-1;
 mock_ota_states[0]=mock_ota_states[2]=ESP_OTA_IMG_VALID;mock_ota_states[1]=ESP_OTA_IMG_ABORTED;
 policy_reset();assert(freslest_ota_boot_begin()&&ready);mock_events=0;
 failed=confirm=restore_confirm=false;previous=NULL;last_percent=-1;error_text[0]=0;atomic_store(&removed,false);
 candidate_from(name);for(unsigned i=0;i<KEY_COUNT;i++)keys[i].edge=false;
}
static void press(unsigned key) {for(unsigned i=0;i<KEY_COUNT;i++)keys[i].edge=i==key;handle_keys();}
static void select_package(void) {press(2);assert(confirm&&!mock_erases&&!mock_writes);}
static void begin_new_boot(void) {mock_running=1;mock_ota_states[1]=ESP_OTA_IMG_PENDING_VERIFY;policy_reset();assert(freslest_ota_boot_begin()&&pending&&!ready);}
static void old_boot(void) {mock_running=0;mock_boot=&mock_parts[0];mock_ota_states[1]=ESP_OTA_IMG_ABORTED;policy_reset();assert(freslest_ota_boot_begin()&&ready);}
static void confirm_startup(void) {for(unsigned i=0;i<60;i++) {mock_time+=200000;freslest_ota_health_tick(true);}assert(ready&&!pending);}
static void capture(const char *path) {FILE *f=fopen(path,"wb");assert(f);assert(fwrite(screen,1,480*320,f)==480*320);fclose(f);}
int main(int argc,char **argv) {
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);original_size=ftell(f);rewind(f);original=malloc(original_size);assert(original);assert(fread(original,1,original_size,f)==original_size);fclose(f);
 mock_flash=malloc(FRESLEST_OS_SLOT_SIZE);screen=calloc(480*320,1);assert(mock_flash&&screen);
 reset("update.zip");valid_package=packages[0].package;draw();capture("update-list.raw");select_package();draw();capture("update-confirm.raw");
 if(setjmp(mock_restart)==0) {press(3);assert(!"BACK must restart");}
 assert(!mock_erases&&!mock_writes&&mock_boot==&mock_parts[0]&&state.mode==IDLE);
 puts("PASS: warning and BACK cancellation make no firmware writes");
 reset("update.zip");unmount_drive();draw();capture("update-insert.raw");
 reset("update.zip");select_package();progress(56,"Writing the inactive OS slot");capture("update-progress.raw");last_percent=-1;
 if(setjmp(mock_restart)==0) {press(2);assert(!"Success must restart");}
 assert(mock_boot==&mock_os&&state.mode==TRIAL&&last_percent==100&&!mock_allocations&&!memcmp(mock_flash,original,original_size));unsigned events=mock_events;
 begin_new_boot();assert(!freslest_ota_inactive_os()&&freslest_ota_request_update()!=ESP_OK);confirm_startup();assert(state.active==1&&state.previous==0&&freslest_ota_previous_os()==&mock_parts[0]);
 assert(freslest_ota_inactive_os()==&mock_parts[0]);assert(freslest_ota_restore_previous()==ESP_OK&&mock_boot==&mock_parts[0]);
 puts("PASS: verified inactive-slot update, trial boot, 10-second/60-frame confirmation and manual restore");
 for(volatile unsigned cut=1;cut<=events;cut++) {
  reset("update.zip");select_package();mock_events=0;mock_cut_at=cut;
  if(setjmp(mock_restart)==0)press(2);
  mock_cut_at=0;
  if(mock_boot==&mock_os) {assert(!memcmp(mock_flash,original,original_size));begin_new_boot();assert(!ready);}
  old_boot();assert(ready&&mock_boot==&mock_parts[0]);
 }
 printf("PASS: %u cuts across every erase/write/read-back/intent/selection boundary preserve the old OS destination\n",events);
 reset("update.zip");select_package();if(setjmp(mock_restart)==0)press(2);begin_new_boot();
 if(setjmp(mock_restart)==0) {mock_time=45000000;deadline->callback(deadline->arg);assert(!"Deadline must roll back");}
 old_boot();assert(ready&&recovered);
 reset("update.zip");select_package();if(setjmp(mock_restart)==0)press(2);begin_new_boot();
 if(setjmp(mock_restart)==0) {freslest_ota_health_tick(false);assert(!"Failed health must roll back");}
 old_boot();assert(ready&&recovered);
 puts("PASS: stalled startup deadline and failed first-boot health return to the prior OS");
 reset("bad-sha.zip");select_package();press(2);assert(failed&&!mock_erases&&!mock_allocations);
 for(unsigned failure=0;failure<7;failure++) {
  reset("update.zip");if(failure==0)mock_heap_fail=true;if(failure==1)mock_nvs_fail=true;if(failure==2)mock_begin_fail=true;if(failure==3)mock_write_failure=2;if(failure==4)mock_end_fail=true;if(failure==5)mock_read_corrupt=true;if(failure==6)mock_boot_fail=true;
  select_package();press(2);assert(failed&&last_percent<100&&mock_boot==&mock_parts[0]&&!mock_allocations);
 }
 puts("PASS: corrupt SHA, heap/NVS, OTA begin/write/end, read-back and boot-selection failures retain old OS");
 reset("update.zip");mock_parts[4].address++;assert(!freslest_ota_layout_valid());mock_parts[4].address--;
 free(original);free(mock_flash);free(screen);return 0;
}
