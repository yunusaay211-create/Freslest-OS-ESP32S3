"""Execute unmodified v5.4.4 selector/bootloader functions with flash doubles."""
from pathlib import Path
import os,re,subprocess
root=Path(__file__).resolve().parents[1];idf=Path(os.environ['IDF_PATH']);build=root/'tests/host_build';build.mkdir(exist_ok=True)
def extract(path,name):
 s=path.read_text();m=re.search(r'^[^\n]*\b'+name+r'\([^;]*?\)\s*\n\{',s,re.M);assert m,name
 end=re.search(r'^}',s[m.end():],re.M);assert end
 return s[m.start():m.end()+end.end()]+'\n'
s='''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <zlib.h>
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_NOT_FOUND 3
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_OTA_MIN 16
#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define SUB_TYPE_ID(i) ((i)&15)
#define FACTORY_INDEX -1
#define INVALID_INDEX -99
#define FLASH_SECTOR_SIZE 4096
+typedef int esp_err_t;
+typedef unsigned esp_partition_subtype_t;
+typedef enum {ESP_OTA_IMG_NEW,ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_VALID,ESP_OTA_IMG_INVALID,ESP_OTA_IMG_ABORTED,ESP_OTA_IMG_UNDEFINED=-1} esp_ota_img_states_t;
+typedef struct {uint32_t ota_seq;uint8_t seq_label[20];uint32_t ota_state,crc;} esp_ota_select_entry_t;
+typedef struct {uint32_t address,size,erase_size;} esp_partition_t;
+typedef struct {uint32_t offset,size;} esp_partition_pos_t;
+typedef struct {esp_partition_pos_t ota_info,factory,ota[3];uint32_t app_count;} bootloader_state_t;
+static uint8_t flash[8192];static unsigned events,cut_at;static jmp_buf power_cut;static bool ota_has_initial_contents;
+static esp_partition_t data_part={0x10000,8192,4096},apps[3];
+static void event(void) {if(++events==cut_at)longjmp(power_cut,1);}
+static uint32_t esp_rom_crc32_le(uint32_t initial,const uint8_t *p,size_t n) {return crc32(initial,p,n);}
+static const esp_partition_t *esp_partition_find_first(int type,int subtype,const char *label) {(void)label;return type==0&&subtype>=16&&subtype<=18?apps+subtype-16:NULL;}
+static esp_err_t esp_partition_erase_range(const esp_partition_t *p,size_t offset,size_t n) {assert(p==&data_part&&offset+n<=8192);event();memset(flash+offset,0xff,n);event();return 0;}
+static esp_err_t esp_partition_write(const esp_partition_t *p,size_t offset,const void *data,size_t n) {assert(p==&data_part&&offset+n<=8192);for(size_t i=0;i<n;i+=4) {event();size_t bytes=n-i;if(bytes>4)bytes=4;memcpy(flash+offset+i,(const uint8_t*)data+i,bytes);event();}return 0;}
+static const esp_partition_t *read_otadata(esp_ota_select_entry_t *out) {memcpy(out,flash,32);memcpy(out+1,flash+4096,32);return &data_part;}
+static esp_err_t bootloader_common_read_otadata(const esp_partition_pos_t *p,esp_ota_select_entry_t *out) {(void)p;read_otadata(out);return 0;}
+static bool esp_flash_encryption_enabled(void) {return false;}
+static esp_err_t write_otadata(const esp_ota_select_entry_t *e,uint32_t address,bool encrypted) {(void)encrypted;esp_partition_erase_range(&data_part,address-0x10000,4096);return esp_partition_write(&data_part,address-0x10000,e,32);}
+int bootloader_common_select_otadata(const esp_ota_select_entry_t*,bool*,bool);
+'''.replace('\n+','\n')
common=idf/'components/bootloader_support/src/bootloader_common_loader.c'
for name in ['bootloader_common_ota_select_crc','bootloader_common_ota_select_invalid','bootloader_common_ota_select_valid','bootloader_common_get_active_otadata','bootloader_common_select_otadata']:s+=extract(common,name)
s+=extract(idf/'components/bootloader_support/src/bootloader_utility.c','bootloader_utility_get_selected_boot_partition')
for name in ['set_new_state_otadata','rewrite_ota_seq','esp_ota_get_app_partition_count','esp_rewrite_ota_data']:s+=extract(idf/'components/app_update/esp_ota_ops.c',name)
s+='''
static void setup(unsigned source) {
 memset(flash,0xff,sizeof(flash));events=cut_at=0;
 esp_ota_select_entry_t old={.ota_seq=source+4,.ota_state=ESP_OTA_IMG_VALID};old.crc=bootloader_common_ota_select_crc(&old);memcpy(flash,&old,32);
 esp_ota_select_entry_t doom={.ota_seq=3,.ota_state=ESP_OTA_IMG_VALID};doom.crc=bootloader_common_ota_select_crc(&doom);memcpy(flash+4096,&doom,32);
}
static int boot(void) {bootloader_state_t bs={.ota_info={0x10000,8192},.app_count=3};return bootloader_utility_get_selected_boot_partition(&bs);}
int main(void) {
 unsigned tested=0;
 for(unsigned source=0;source<2;source++) {
  unsigned target=source^1;setup(source);assert(esp_rewrite_ota_data(16+target)==0);unsigned count=events;
  assert(boot()==(int)target);assert(boot()==(int)source);
  for(volatile unsigned cut=1;cut<=count;cut++) {
   setup(source);cut_at=cut;if(setjmp(power_cut)==0)esp_rewrite_ota_data(16+target);cut_at=0;
   int next=boot();assert(next==(int)source||next==(int)target);if(next==(int)target)assert(boot()==(int)source);++tested;
  }
  // Confirmed images persist across resets. First-boot NEW/PENDING images do not.
  setup(source);assert(esp_rewrite_ota_data(16+target)==0);assert(boot()==(int)target);
  esp_ota_select_entry_t entries[2];read_otadata(entries);int active=bootloader_common_get_active_otadata(entries);assert(active>=0);
  entries[active].ota_state=ESP_OTA_IMG_VALID;assert(rewrite_ota_seq(entries,entries[active].ota_seq,active,&data_part)==0);assert(boot()==(int)target);assert(boot()==(int)target);
 }
 printf("PASS: actual IDF selector and bootloader rollback, both A/B directions, %u sector/word-boundary power cuts, Doom never chosen\\n",tested);return 0;
}
'''
p=build/'idf_rollback_cases.c';p.write_text(s);exe=build/'test_idf_rollback'
subprocess.run(['gcc','-std=gnu17','-O1','-g','-fsanitize=address,undefined',str(p),'-lz','-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
