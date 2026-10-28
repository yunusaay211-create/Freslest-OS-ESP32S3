// Preset boot must migrate old LCD choices once and retain later recovery choices.
#include <stdio.h>
#include "vga_host.h"
#include "../shared_components/freslest_video/freslest_video.c"
static void reboot(void){boot_read=false;back=1;nvs_failure=0;}
static void fresh(void){reboot();stored_v2=0;stored_preset252=0;}
static int seen(void){return stored_preset252;}
int main(void){
 fresh();assert(freslest_video_boot_output()==FRESLEST_DISPLAY_VGA);
 assert(stored_v2==2&&seen()==1);
 assert(freslest_video_save_output(FRESLEST_DISPLAY_LCD));reboot();
 assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD&&stored_v2==0&&seen()==1);
 assert(freslest_video_save_output(FRESLEST_DISPLAY_VGA));reboot();
 assert(freslest_video_boot_output()==FRESLEST_DISPLAY_VGA);
 // BACK must take priority over both a new preset and an existing VGA choice.
 fresh();back=0;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD);
 assert(stored_v2==0&&seen()==1);reboot();assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD);
 assert(freslest_video_save_output(FRESLEST_DISPLAY_VGA));reboot();back=0;
 assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD&&stored_v2==0);
 for(int failure=1;failure<=4;failure++){
  fresh();nvs_failure=failure;assert(freslest_video_boot_output()==FRESLEST_DISPLAY_VGA);
  // A failed NVS call must not block the requested runtime output. Real NVS
  // does not guarantee rollback of two keys; do not assert crash atomicity.
  fresh();assert(freslest_video_boot_output()==FRESLEST_DISPLAY_VGA&&seen()==1&&stored_v2==2);
  fresh();stored_v2=2;back=0;nvs_failure=failure;
  assert(freslest_video_boot_output()==FRESLEST_DISPLAY_LCD);
 }
 fresh();assert(freslest_video_begin());
 assert(saved_config.timings.pclk_hz==25200000);
 assert(saved_config.timings.h_res==640&&saved_config.timings.v_res==480);
 assert(saved_config.timings.hsync_pulse_width==96&&saved_config.timings.hsync_back_porch==48&&saved_config.timings.hsync_front_porch==16);
 assert(saved_config.timings.vsync_pulse_width==2&&saved_config.timings.vsync_back_porch==33&&saved_config.timings.vsync_front_porch==10);
 puts("PASS: VGA preset migration, user choice, BACK recovery, NVS error handling and driver clock");
 return 0;
}
