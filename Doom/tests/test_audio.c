#include <assert.h>
#include <stdio.h>
#include "host_idf.h"
int64_t host_now;
unsigned host_delay_calls,host_pwm_updates;
int host_gpio[64],host_ledc_fail;
uint32_t host_duty,host_freq;
#include "../components/prboom-esp32-compat/i_sound.c"
#undef malloc
#undef calloc
#undef free

typedef struct { uint8_t bytes[10]; int64_t time; } sent_t;
static sent_t sent[4096];
static unsigned sent_count;
static uint8_t rx[4096];
static unsigned rx_count;
static int auto_status = 1;
static uint8_t *sample;
static int sample_length;
int snd_MusicVolume = 12;
int numChannels=8,default_numChannels=8;
sfxinfo_t S_sfx[NUMSFX];
musicinfo_t S_music[NUMMUSIC];

static void reply(uint8_t cmd,uint16_t value)
{
 uint8_t p[10]={0x7e,0xff,6,cmd,0,(uint8_t)(value>>8),(uint8_t)value,0,0,0xef};
 uint16_t checksum=(uint16_t)(0u-(p[1]+p[2]+p[3]+p[4]+p[5]+p[6]));
 p[7]=checksum>>8;p[8]=checksum;
 assert(rx_count+10<=sizeof(rx));memcpy(rx+rx_count,p,10);rx_count+=10;
}
int uart_write_bytes(int n,const void *p,size_t len)
{
 (void)n;assert(len==10);assert(sent_count<4096);
 const uint8_t *b=p;
 uint16_t check=(uint16_t)(0u-(b[1]+b[2]+b[3]+b[4]+b[5]+b[6]));
 assert(b[0]==0x7e&&b[9]==0xef&&check==((uint16_t)b[7]<<8|b[8]));
 memcpy(sent[sent_count].bytes,p,10);sent[sent_count++].time=host_now;
 if(auto_status&&b[3]==0x42) reply(0x42,1);
 return (int)len;
}
int uart_read_bytes(int n,void *p,uint32_t len,TickType_t timeout)
{
 (void)n;(void)timeout;unsigned take=rx_count<len?rx_count:len;
 memcpy(p,rx,take);memmove(rx,rx+take,rx_count-take);rx_count-=take;return (int)take;
}
esp_err_t uart_flush_input(int n) { (void)n;rx_count=0;return 0; }
int (W_CheckNumForName)(const char *name, int ns) { (void)name;(void)ns;return 1; }
int W_LumpLength(int lump) { (void)lump;return sample_length; }
const void *W_CacheLumpNum(int lump) { (void)lump;return sample; }
static unsigned plays(unsigned track)
{
 unsigned found=0;for(unsigned i=0;i<sent_count;++i)
  if(sent[i].bytes[3]==0x12&&(((unsigned)sent[i].bytes[5]<<8)|sent[i].bytes[6])==track) ++found;
 return found;
}
static void run_ms(unsigned ms)
{
 for(unsigned i=0;i<ms;i+=20){host_now+=20000;df_worker_step(host_now);}
}
static void reset_music(void)
{
 sent_count=rx_count=0;host_now=0;df_next_command_us=0;
 df_ready=1;df_card_present=1;df_media_serial=df_status_serial=0;
 df_rx_used=df_rx_errors=df_end_pending=0;
 memset(&df_request,0,sizeof(df_request));df_request.volume=24;df_request.revision=1;
 memset(&df_state,0,sizeof(df_state));df_state.revision=UINT32_MAX;df_state.volume=24;
 auto_status=1;host_delay_calls=0;
}
static void test_music(void)
{
 S_music[mus_e1m1].name="e1m1";S_music[mus_e1m2].name="e1m2";
 S_music[mus_e1m3].name="e1m3";S_music[mus_inter].name="inter";
 reset_music();I_PlaySong(mus_e1m1,1);
 assert(sent_count==0&&host_delay_calls==0);run_ms(1600);assert(plays(70)==1);
 unsigned old_plays=plays(70);
 I_StopSong(mus_e1m1);I_PlaySong(mus_inter,1);I_StopSong(mus_inter);I_PlaySong(mus_e1m2,1);
 assert(host_delay_calls==0);run_ms(1600);
 assert(plays(71)==1&&plays(70)==old_plays&&plays(97)==0);
 reply(0x3d,167); /* completion uses a FAT index, not /MP3 track number */
 run_ms(20);int64_t ended=host_now;run_ms(980);assert(plays(71)==1);
 run_ms(1600);assert(plays(71)==2&&plays(70)==old_plays);
 int64_t repeated=0;for(unsigned k=0;k<sent_count;++k)if(sent[k].bytes[3]==0x12&&sent[k].bytes[6]==71)repeated=sent[k].time;
 assert(repeated-ended>=1000000);
 for(unsigned i=0;i<sent_count;++i){
  assert(!(sent[i].bytes[3]==0x19&&sent[i].bytes[6]==0));
  if(i)assert(sent[i].time-sent[i-1].time>=DF_COMMAND_US);
 }
 I_PauseSong(mus_e1m2);run_ms(300);assert(df_state.paused==1);
 unsigned paused_plays=plays(71);reply(0x3d,167);run_ms(600);assert(plays(71)==paused_plays);
 I_ResumeSong(mus_e1m2);run_ms(300);assert(df_state.paused==0);
 I_SetMusicVolume(0);assert(host_delay_calls==0);run_ms(200);assert(df_state.volume==0);
 I_SetMusicVolume(99);run_ms(200);assert(df_state.volume==30);
 I_StopSong(mus_e1m2);run_ms(500);reply(0x3d,167);run_ms(500);assert(plays(71)==paused_plays);
 puts("PASS: E1M1 -> intermission -> E1M2, repeat, pause, volume, stop; nonblocking Doom APIs");

 reset_music();I_PlaySong(mus_e1m1,1);run_ms(260);I_PlaySong(mus_e1m3,1);run_ms(1600);
 assert(plays(72)==1);unsigned p70=plays(70);reply(0x3d,70);run_ms(2600);assert(plays(70)==p70&&plays(72)==2);
 reply(0x3b,2);run_ms(300);I_PlaySong(mus_e1m2,1);run_ms(1000);assert(plays(71)==0);
 reply(0x3a,2);run_ms(1600);assert(plays(71)==1);
 puts("PASS: superseded transition, SD removal/reinsertion uses current map");

 reset_music();auto_status=0;I_PlaySong(mus_e1m2,0); /* menu-style one-shot request also repeats */run_ms(1600);assert(plays(71)==1&&df_state.phase==5);
 reply(0x3d,71);rx[0]=0;run_ms(200);assert(plays(71)==1);
 reply(0x3d,71);run_ms(2600);assert(plays(71)==2);
 puts("PASS: missing status reply, corrupt UART packet recovery");
 reset_music();I_PlaySong(mus_e1m2,1);auto_status=0;run_ms(1000);
 assert(df_state.phase==4);reply(0x42,0);run_ms(280);assert(plays(71)==2);
 puts("PASS: stopped/busy response retries playback");
}
static void test_pwm(void)
{
 assert(pwm_sfx_init()==1);assert(sfx_timer&&!sfx_timer->running);
 unsigned delays=host_delay_calls;int64_t before=host_now;
 pwm_sfx_start();assert(sfx_ready&&sfx_timer_running&&host_freq==30000);
 assert(host_now==before&&host_delay_calls==delays);
 assert(sfx_timer->period==125&&sfx_timer->args.skip_unhandled_events==1);
 S_sfx[sfx_pistol].lumpnum=1;
 assert(I_StartSound(sfx_pistol,0,127,128,128,64)==0);
 uint32_t minimum=1023,maximum=0;unsigned ticks=0;
 while(I_SoundIsPlaying(0)&&ticks++<20000){pwm_mix_callback(NULL);if(host_duty<minimum)minimum=host_duty;if(host_duty>maximum)maximum=host_duty;}
 assert(ticks>100&&ticks<20000&&minimum<512&&maximum>512&&maximum<=1023&&sfx_pwm_errors==0);
 assert(I_StartSound(sfx_pistol,0,0,128,128,64)==0);
 for(int i=0;i<100;++i){pwm_mix_callback(NULL);assert(host_duty==512);}
 I_StopSound(8);assert(I_SoundIsPlaying(0));I_StopSound(0);assert(!I_SoundIsPlaying(0));
 assert(I_StartSound(sfx_pistol,8,127,128,128,64)==-1);
 numChannels=32;I_SetChannels();assert(numChannels==8&&default_numChannels==8);
 uint8_t *long_pcm=calloc(70000,1);assert(long_pcm);
 sfx_voices[0]=(pwm_voice_t){.pcm=long_pcm,.count=70000,.phase=65535ULL<<16,.step=65536,.gain=0,.active=1};
 pwm_mix_callback(NULL);assert(sfx_voices[0].phase==(65536ULL<<16));I_StopSound(0);free(long_pcm);
 pwm_sfx_shutdown();host_ledc_fail=1;assert(pwm_sfx_init()==0);host_ledc_fail=0;
 puts("PASS: GPIO21 direct duty, silent boot/30kHz carrier, real WAD pistol PCM, mute, handle bounds, long sample phase");
}
int main(int argc,char **argv)
{
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);sample_length=(int)ftell(f);rewind(f);
 sample=malloc(sample_length);assert(sample);assert(fread(sample,1,sample_length,f)==(size_t)sample_length);fclose(f);
 test_music();test_pwm();free(sample);return 0;
}
