#pragma once
#include <stdint.h>
#include <stddef.h>
// Externally synchronized. Fixed storage; neither playback nor writes allocate.
class NesPcmQueue {
public:
  static constexpr size_t capacity=2048,prefill=384;
  void reset() {read_=write_=count_=0;primed_=false;dropped_=underruns_=0;}
  size_t push(const int16_t* pcm,size_t count) {
    if(!pcm)return 0;
    size_t accepted=count;
    if(accepted>capacity-count_)accepted=capacity-count_;
    for(size_t i=0;i<accepted;i++) {data_[write_]=pcm[i];write_=(write_+1)%capacity;}
    count_+=accepted;dropped_+=(uint32_t)(count-accepted);return accepted;
  }
  bool pop(int16_t& out) {
    if(!primed_&&count_>=prefill)primed_=true;
    if(primed_&&count_) {out=data_[read_];read_=(read_+1)%capacity;--count_;return true;}
    if(primed_) {++underruns_;primed_=false;}
    out=0;return false;
  }
  size_t count() const {return count_;}
  uint32_t dropped() const {return dropped_;}
  uint32_t underruns() const {return underruns_;}
private:
  int16_t data_[capacity]{};
  size_t read_=0,write_=0,count_=0;
  uint32_t dropped_=0,underruns_=0;
  bool primed_=false;
};
