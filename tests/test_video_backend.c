#include "video_stubs/host_video.h"
#include "../shared_components/freslest_video/freslest_video.c"
#include <stdio.h>
static void reset(void){assert(!live&&!live_panels&&!live_sem);output=0;boot_read=started=first_frame_ready=false;vsyncs=presentations=last_vsync_us=unchanged_frames=0;back=1;nvs_fail=stage=alloc_fail=allocations=task_calls=0;configured_pins=host_time=started_us=0;stored=0;stored_tv_size=stored_color_size=stored_color_v2_size=0;color=(freslest_tv_color_t){0,100,0};color_revision=rendered_color_revision=1;tv=(freslest_tv_config_t){0,0,100};}
int main(void){
 reset();assert(freslest_video_boot_output()==0&&freslest_video_begin()&&freslest_video_healthy()&&!task_calls);
 reset();stored=99;assert(freslest_video_boot_output()==0);
 reset();stored=1;back=0;assert(freslest_video_boot_output()==0&&!stored);
 for(int i=1;i<=3;i++){reset();nvs_fail=i;assert(!freslest_video_save_output(1));}
 reset();assert(!freslest_video_save_output(9));assert(freslest_video_save_output(1)&&stored==1);
 for(int i=1;i<=3;i++){reset();nvs_fail=i;assert(!freslest_video_tv_save((freslest_tv_config_t){8,-4,90}));assert(tv.scale==100);}
 reset();assert(freslest_video_tv_save((freslest_tv_config_t){127,-128,80}));assert(tv.x==32&&tv.y==-24&&tv.scale==80);
 tv=(freslest_tv_config_t){0,0,100};assert(freslest_video_boot_output()==0&&tv.x==32&&tv.y==-24&&tv.scale==80);
 reset();stored_tv_size=2;assert(freslest_video_boot_output()==0&&tv.scale==100);
 reset();stored_tv_size=3;stored_tv[2]=0;assert(freslest_video_boot_output()==0&&tv.scale==100);
 for(int i=1;i<=3;i++){reset();nvs_fail=i;assert(!freslest_video_color_save((freslest_tv_color_t){35,130,0}));assert(color.hue==0&&color.saturation==100);}
 reset();assert(freslest_video_color_save((freslest_tv_color_t){999,200,22}));assert(color.hue==180&&color.saturation==160&&!color.detail);
 color=(freslest_tv_color_t){0,100,0};assert(freslest_video_boot_output()==0&&color.hue==180&&color.saturation==160);
 reset();stored_color_size=3;assert(freslest_video_boot_output()==0&&color.hue==0&&color.saturation==100);
 // Migrate v1 without changing it; v2 detail survives save/reboot and has
 // priority. Invalid v2 falls back to v1; old apps retain the original blob.
 reset();freslest_tv_color_t legacy={-10,95,0};memcpy(stored_color,&legacy,4);stored_color_size=4;
 assert(freslest_video_boot_output()==0&&color.hue==-10&&color.saturation==95&&color.detail==FRESLEST_TV_CLEAN);
 assert(freslest_video_color_save((freslest_tv_color_t){25,120,FRESLEST_TV_SHARP}));
 assert(!memcmp(stored_color,&legacy,4)&&stored_color_v2_size==4);
 boot_read=false;assert(freslest_video_boot_output()==0&&color.hue==25&&color.detail==FRESLEST_TV_SHARP);
 stored_color_v2[3]=2;boot_read=false;assert(freslest_video_boot_output()==0&&color.hue==-10&&color.detail==FRESLEST_TV_CLEAN);
 reset();freslest_tv_color_t bad={0,161,0};memcpy(stored_color,&bad,4);stored_color_size=4;assert(freslest_video_boot_output()==0&&color.saturation==100);
 for(int i=1;i<=7;i++){reset();stage=i;stored=1;assert(!freslest_video_begin());assert(!live&&!live_panels&&!live_sem&&!started&&!wave);}
 for(int i=1;i<=3;i++){reset();alloc_fail=i;stored=1;assert(!freslest_video_begin());assert(!live&&!live_panels&&!live_sem);}
 reset();stored=1;assert(freslest_video_begin()&&started&&live==3&&live_panels==1&&live_sem==1);
 assert(!(configured_pins&((1ULL<<47)|(1ULL<<9))));
 // No frame is confirmed before a complete source has actually traversed DMA.
 assert(!engine->ownership.swaps&&!freslest_video_healthy());
 assert(!freslest_video_healthy()&&freslest_video_starting());
 host_time=started_us+10000000;assert(!freslest_video_starting()&&!freslest_video_healthy());host_time=started_us;
 uint32_t pal[256]={0};pal[1]=255;uint8_t *src=malloc(320*240);memset(src,1,320*240);
 freslest_video_indexed(src,320,240,pal,false,0,0);int slot=video_slots_consume(&slots);assert(slot>=0&&frames[slot][0]==224);
 freslest_video_indexed(src,320,240,pal,false,0,0);
 assert(slots.ready==-1&&slots.writing==-1&&unchanged_frames==1);
 memset(src,0,320*240);assert(frames[slot][0]==224); // source ownership ends at return
 freslest_video_indexed(src,320,240,pal,false,0,0);assert(slots.ready>=0&&slots.ready!=slot);
 render_pending();for(int i=0;i<3;i++)host_av_field(engine);assert(freslest_video_healthy()&&!freslest_video_starting());
 host_time+=500001;assert(!freslest_video_healthy()&&!freslest_video_starting());
 // Recolour the current owned frame even when the UI pixels are unchanged.
 memset(src,1,320*240);freslest_video_indexed(src,320,240,pal,false,0,0);render_pending();
 host_av_field(engine);
 uint8_t before_color=wave[AV_ACTIVE_TOP*AV_LINE+AV_ACTIVE_START];uint32_t before_count=presentations;
 uint32_t rev=color_revision;freslest_video_color_preview(freslest_video_color_config());assert(color_revision==rev);
 freslest_video_color_preview((freslest_tv_color_t){90,100,0});assert(color_revision==rev+1);render_pending();
 assert(presentations==before_count+1&&wave[AV_ACTIVE_TOP*AV_LINE+AV_ACTIVE_START]!=before_color);
 render_pending();assert(presentations==before_count+1);host_av_field(engine);
 // Detail changes re-encode the current image even without a new source.
 before_count=presentations;freslest_video_color_preview((freslest_tv_color_t){90,100,FRESLEST_TV_SHARP});render_pending();assert(presentations==before_count+1);
 freslest_av_close(engine);engine=NULL;vSemaphoreDelete(sync_sem);sync_sem=NULL;for(int i=0;i<3;i++)heap_caps_free(frames[i]);free(src);reset();
 puts("PASS: real backend LCD default/invalid preference, BACK recovery, TV alignment and colour v1/v2 migration and legacy preservation, detail/monochrome persistence/clamping/NVS errors, immutable field publication and stationary-frame recolour/detail, all allocation/driver/task failure cleanup, original GPIO mapping, source ownership, bounded first-frame wait and immediate later stalled-DMA health");
}
