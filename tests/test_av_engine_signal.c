#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_signal.h"
#include "av_ownership.h"
static unsigned phase_pixel(const uint8_t *row,const uint8_t palette[256][4],int x,unsigned phase){
 if(x<0)x=0;if(x>=AV_ACTIVE_SAMPLES)x=AV_ACTIVE_SAMPLES-1;
 uint64_t fixed=(uint64_t)x*(AV_WIDTH-1)*256/(AV_ACTIVE_SAMPLES-1);
 unsigned at=fixed/256,f=fixed%256,next=at+1<AV_WIDTH?at+1:at;
 return (palette[row[at]][phase]*(256-f)+palette[row[next]][phase]*f+128)/256;
}
int main(void){
 assert(AV_FIELD_BYTES==238420&&AV_LINE==910&&AV_LINES==262&&AV_FIELD_BYTES%4==0);
 uint8_t *allocation=malloc(AV_FIELD_BYTES+66),*wave=allocation+33,*before=malloc(AV_FIELD_BYTES);
 uint8_t *canvas=malloc(AV_WIDTH*AV_HEIGHT+32);av_renderer_t *renderer=malloc(sizeof(*renderer));
 assert(allocation&&before&&canvas&&renderer);memset(allocation,0xa5,AV_FIELD_BYTES+66);memset(canvas,0xa5,AV_WIDTH*AV_HEIGHT+32);
 av_signal_prime(wave,true);
 const int burst[]={36,0,-36,0};
 for(int y=0;y<AV_LINES;y++)for(int x=0;x<AV_LINE;x++){
  int expected=AV_BLANK;
  if(y<9){int low=y>=3&&y<6?388:33;if(x%455<low)expected=0;}
  else if(x<67)expected=0;
  else if(x>=75&&x<115)expected+=burst[(x+2*y)%4];
  else if(y>=20&&y<260&&x>=136&&x<880)expected=AV_BLACK;
  assert(wave[y*AV_LINE+x]==expected);
 }
 memcpy(before,wave,AV_FIELD_BYTES);av_signal_burst(wave,false);
 for(int y=0;y<AV_LINES;y++)for(int x=0;x<AV_LINE;x++)
  assert(wave[y*AV_LINE+x]==(y>=9&&x>=75&&x<115?AV_BLANK:before[y*AV_LINE+x]));
 av_signal_burst(wave,true);assert(!memcmp(wave,before,AV_FIELD_BYTES));
 uint8_t palette[256][4];av_signal_palette(palette,0,100);
 for(int n=0;n<8;n++){int i=(n<<5)|(n<<2)|(n>>1);for(int k=1;k<4;k++)assert(palette[i][k]==palette[i][0]);}
 av_renderer_init(renderer);unsigned state=71;
 for(int i=0;i<AV_WIDTH*AV_HEIGHT;i++){state=state*1664525+1013904223;canvas[i]=state>>24;}
 for(unsigned detail=0;detail<=1;detail++){
  av_signal_encode(renderer,wave,canvas,palette,detail);
  for(int y=0;y<AV_HEIGHT;y++)for(int x=0;x<AV_ACTIVE_SAMPLES;x++){
   unsigned phase=(x+136+2*(y+20))%4,expected=0;
   if(detail)expected=phase_pixel(canvas+y*AV_WIDTH,palette,x,phase);
   else{for(int k=-1;k<=2;k++)expected+=phase_pixel(canvas+y*AV_WIDTH,palette,x+k,phase);expected=(expected+2)/4;}
   assert(wave[(y+20)*910+136+x]==expected);
  }
  for(int y=0;y<AV_LINES;y++)for(int x=0;x<AV_LINE;x++)
   if(y<20||y>=260||x<136||x>=880)assert(wave[y*910+x]==before[y*910+x]);
 }
 // A real receiver's one-line comb must recover a flat colour vector.
 const unsigned colors[]={224,28,3,252,31,227,0,255,109};
 for(unsigned c=0;c<sizeof(colors)/sizeof(*colors);c++)for(unsigned detail=0;detail<=1;detail++){
  memset(canvas,colors[c],AV_WIDTH*AV_HEIGHT);av_signal_encode(renderer,wave,canvas,palette,detail);
  for(int y=0;y<240;y++)for(int x=0;x<744;x++)assert(wave[(y+20)*910+136+x]==palette[colors[c]][(136+x+2*(y+20))%4]);
 }
 for(unsigned p=0;p<4;p++){
  av_signal_pattern(canvas,p);
  for(int i=0;i<AV_WIDTH*AV_HEIGHT;i++)if(p!=1){unsigned v=canvas[i];assert((v>>5)==((v>>2)&7)&&(v&3)==(v>>5)/2);}
 }
 for(int i=0;i<33;i++)assert(allocation[i]==0xa5&&wave[AV_FIELD_BYTES+i]==0xa5);
 for(int i=0;i<32;i++)assert(canvas[AV_WIDTH*AV_HEIGHT+i]==0xa5);
 av_ownership_t q;av_ownership_init(&q);
 for(int iteration=0;iteration<10000;iteration++){
  int old=q.active,b=av_ownership_acquire(&q);assert(b>=0&&b!=old);
  assert(av_ownership_queue(&q,b));int c=av_ownership_acquire(&q);assert(c>=0&&c!=old&&c!=b);
  assert(!av_ownership_queue(&q,c)&&q.writing==c);
  for(int delay=0;delay<3;delay++){av_ownership_complete(&q,old);assert(q.active==old&&q.pending==b);}
  av_ownership_complete(&q,b);assert(q.active==b&&q.pending==-1);
  assert(av_ownership_queue(&q,c));av_ownership_complete(&q,b);assert(q.pending==c&&q.active==b);
  av_ownership_complete(&q,c);assert(q.active==c&&q.pending==-1);
 }
 assert(q.swaps==20000);
 puts("PASS: all 238420 physical samples, 910x262 field continuity, pre/post equalization and serrations, reversible burst, scalar interpolation/trap oracle on every sample, flat-colour phase, mono lab patterns and unaligned destination guards");
 puts("PASS: 20000 immutable-field handovers with delayed old-ring EOF, a third prepared field, busy retry, protected active/pending slots and no premature buffer reuse");
 free(renderer);free(canvas);free(before);free(allocation);
}
