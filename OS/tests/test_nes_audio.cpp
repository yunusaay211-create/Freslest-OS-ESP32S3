#include "nes_audio.h"
#include "nes_pcm_queue.h"
#include "mock_audio.h"
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <future>
#include <thread>
int main() {
 NesPcmQueue q;int16_t values[NesPcmQueue::capacity+1024];
 for(size_t i=0;i<sizeof(values)/2;i++)values[i]=(int16_t)i;
 int16_t out;
 assert(q.push(values,383)==383&&!q.pop(out));assert(q.push(values+383,1)==1);
 for(int i=0;i<384;i++){assert(q.pop(out));assert(out==i);}
 assert(!q.pop(out)&&q.underruns()==1);assert(!q.pop(out)&&q.underruns()==1);
 for(unsigned round=0;round<12;round++) {
  assert(q.push(values,NesPcmQueue::capacity)==NesPcmQueue::capacity);
  for(unsigned i=0;i<NesPcmQueue::capacity;i++){assert(q.pop(out));assert(out==(int16_t)i);}
 }
 q.reset();assert(q.push(values,NesPcmQueue::capacity+1024)==NesPcmQueue::capacity&&q.dropped()==1024);
 nesAudioStop();nesAudioSetVolume(true,255);assert(nesAudioStart());
 std::fill_n(values,2048,12000);nesAudioSubmit(values,384);mockAudioTick(40);
 assert(mockDuties.back()==699);nesAudioSetVolume(false,255);mockAudioTick(1);assert(mockDuties.back()==512);
 nesAudioSetVolume(true,128);mockAudioTick(40);assert(mockDuties.back()>=605&&mockDuties.back()<=607);
 mockAudioTick(2000);assert(mockDuties.back()==512&&nesAudioStats().underruns==1);
 std::fill_n(values,3072,(int16_t)-32768);nesAudioSubmit(values,3072);assert(nesAudioStats().queued==2048&&nesAudioStats().dropped==1024);
 mockAudioTick(20);assert(mockDuties.back()<512);
 auto stale=mockCallback();nesAudioStop();assert(!nesAudioStats().running&&mockDuties.back()==512);
 assert(nesAudioStart());nesAudioSubmit(values,384);auto size=mockDuties.size();stale.callback(stale.arg);assert(mockDuties.size()==size&&nesAudioStats().queued==384);
 mockFailWrite=true;mockAudioTick(1);mockFailWrite=false;assert(nesAudioStats().hardwareErrors==1);
 // Force an in-flight driver write while the main task closes the game.
 mockBlockWrite(true);std::thread consumer([]{mockAudioTick(1);});mockWaitForBlockedWrite();
 auto close=std::async(std::launch::async,[]{nesAudioStop();});
 assert(close.wait_for(std::chrono::milliseconds(10))==std::future_status::timeout);
 mockBlockWrite(false);consumer.join();assert(close.wait_for(std::chrono::seconds(2))==std::future_status::ready);close.get();
 assert(mockDuties.back()==512&&!nesAudioStats().running&&nesAudioStats().queued==0);
 mockFailPwm=true;assert(!nesAudioStart());mockFailPwm=false;
 mockFailCreate=true;assert(!nesAudioStart());mockFailCreate=false;
 mockFailStart=true;assert(!nesAudioStart());mockFailStart=false;
 assert(mockCreated==mockDeleted);assert(nesAudioStart());nesAudioStop();assert(mockCreated==mockDeleted);
 puts("PASS: PCM queue prefill/wrap/overflow, exact GPIO21 timer/PWM setup, mute/gain, underrun fade, stale callbacks, in-flight shutdown and failed-device cleanup");
}
