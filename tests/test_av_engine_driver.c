#include "../shared_components/freslest_av/av_engine.c"
#include <stdio.h>
static void no_leaks(void){assert(!mock_memory&&!mock_channels&&!mock_lists&&!mock_claims&&!mock_power_refs&&!mock_sources&&!mock_locks);}
static void eof(freslest_av_handle_t e,int field){mock_time+=16652;gdma_event_data_t event={.tx_eof_desc_addr=e->tail[field]};e->dma->callbacks.on_trans_eof(e->dma,&event,e);}
static dma_descriptor_t *field_bytes(freslest_av_handle_t e,dma_descriptor_t *first,const uint8_t *expected){
 dma_descriptor_t *p=first;size_t n=0;
 for(unsigned i=0;i<60;i++){
  assert(p->dw0.owner==1&&p->dw0.suc_eof==(i==59));
  assert(n+p->dw0.length<=AV_FIELD_BYTES&&!memcmp(p->buffer,expected+n,p->dw0.length));
  n+=p->dw0.length;if(i==59){int slot=-1;for(int k=0;k<3;k++)if(e->tail[k]==(intptr_t)p)slot=k;assert(slot>=0);eof(e,slot);}
  p=p->next;
 }
 assert(n==AV_FIELD_BYTES);return p;
}
int main(void){
 freslest_av_handle_t e=NULL;assert(freslest_av_open(NULL,NULL)==ESP_ERR_INVALID_ARG);
 assert(freslest_av_open(NULL,&e)==ESP_OK);int success_ops=mock_ops;
 assert(!mock_lcd.rgb&&!mock_lcd.convert&&!mock_lcd.swizzle&&!mock_lcd.reverse&&!mock_lcd.next&&mock_lcd.always);
 assert(mock_lcd.stride==8&&!mock_lcd.cmd&&mock_lcd.dummy==1&&mock_lcd.data==1&&mock_routes==8);
 assert(mock_lcd.divider==8&&mock_lcd.denom==63&&mock_lcd.numer==24);
 assert(!freslest_av_healthy(e));
 uint8_t *original=malloc(AV_FIELD_BYTES),*new_field=malloc(AV_FIELD_BYTES);memcpy(original,e->wave[0],AV_FIELD_BYTES);
 dma_descriptor_t *head=(void*)e->dma->head;
 // Model a prefetched old tail BEFORE a new field is published. One more
 // old field may be output; its bytes must remain immutable until NEW EOF.
 dma_descriptor_t *prefetched=((dma_descriptor_t*)e->tail[0])->next;
 uint8_t *w;int slot=freslest_av_acquire(e,&w);assert(slot==1);memset(w,0x31,AV_FIELD_BYTES);memcpy(new_field,w,AV_FIELD_BYTES);
 assert(freslest_av_publish(e,slot));head=field_bytes(e,head,original);assert(e->ownership.active==0&&e->ownership.pending==1);
 int writing=freslest_av_acquire(e,&w);assert(writing==2);assert(!freslest_av_publish(e,writing)&&e->ownership.writing==2);
 head=field_bytes(e,prefetched,original);assert(e->ownership.active==0&&e->ownership.pending==1);
 head=field_bytes(e,head,new_field);assert(e->ownership.active==1&&e->ownership.pending==-1&&freslest_av_healthy(e));
 assert(!memcmp(e->wave[0],original,AV_FIELD_BYTES));
 freslest_av_cancel(e,writing);assert(e->ownership.writing==-1);
 mock_time+=500001;assert(!freslest_av_healthy(e));eof(e,1);assert(freslest_av_healthy(e));
 mock_time=(int64_t)UINT32_MAX-20;eof(e,1);mock_time+=100;assert(freslest_av_healthy(e));
 gdma_event_data_t bad={.tx_eof_desc_addr=7};e->dma->callbacks.on_trans_eof(e->dma,&bad,e);assert(!freslest_av_healthy(e)&&e->stats.unknown_eof==1);
 e->dma->callbacks.on_descr_err(e->dma,&bad,e);assert(e->stats.descriptor_errors==1);
 mock_lcd.lcd_user.lcd_start=0;freslest_av_stats_t stats;freslest_av_stats(e,&stats);assert(!stats.running);
 freslest_av_close(e);e=NULL;free(original);free(new_field);no_leaks();
 // Every fallible initialization boundary must release channels, all field
 // buffers/lists, clock references, bus claims and PM locks.
 for(int fail=1;fail<=success_ops;fail++){
  mock_ops=0;mock_fail_at=fail;mock_routes=0;
  assert(freslest_av_open(NULL,&e)!=ESP_OK&&!e);no_leaks();
 }
 mock_fail_at=0;
 puts("PASS: actual raw driver configuration, original 8 GPIOs, exact 60-node/238420-byte rings, 64-byte PSRAM alignment, data-popped EOF, prefetched old-field delay without mixed data, confirmed-new-field ownership, clock wrap/stall/error health and stopped-stream detection");
 printf("PASS: actual driver cleanup at all %d fallible initialization boundaries, including all three field buffers/rings, source/bus references, GDMA and PM locks\n",success_ops);
}
