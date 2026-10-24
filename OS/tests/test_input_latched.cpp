#include "../freslest/input.cpp"
#include <cassert>
#include <cstdio>
#include <utility>
#include <initializer_list>
uint32_t testMillis=0;
void audioClick(){}
static void keys(uint8_t mask) {
  for(unsigned i=0;i<7;++i)testPins[pins[i]]=(mask&(1u<<i))?LOW:HIGH;
}
static void advance(unsigned count,uint8_t mask) {
  keys(mask);
  for(unsigned i=0;i<count;i+=2){testMillis+=2;sampleButtons(testMillis);}
}
static void reset() {
  samplerStarted=false;keys(0);inputBegin();samplerStarted=true;
  inputNesBegin();assert(inputNesPad()==0);
}
int main() {
  reset();
  // A 50ms tap happens entirely inside a 200ms LCD/UI stall.
  advance(40,0);advance(50,KeyOk);advance(110,0);
  inputUpdate(testMillis);assert(bOk.pressed()&&!bOk.down());
  assert(inputNesPad()==1);assert(inputNesPad()==0);
  inputUpdate(testMillis);assert(!bOk.pressed());
  for(auto pair: {std::pair<uint8_t,uint8_t>{KeyRight,0x80},{KeyLeft,0x40},{KeyUp,0x10},{KeyDown,0x20},{KeyFire,0x02}}) {
    reset();advance(30,pair.first);
    for(int frame=0;frame<20;++frame)assert(inputNesPad()==pair.second);
    advance(30,0);assert(inputNesPad()==0);
  }
  reset();advance(30,KeyUp);advance(30,KeyUp|KeyOk);assert(inputNesPad()==8);
  advance(30,0);assert(inputNesPad()==0);
  reset();advance(30,KeyOk);advance(30,KeyDown|KeyOk);assert(inputNesPad()==4);
  advance(30,0);assert(inputNesPad()==0);
  // Separate taps must not become a START chord after a long frame.
  reset();advance(30,KeyUp);advance(30,0);advance(30,KeyOk);advance(30,0);
  assert((inputNesPad()&8)==0);
  reset();advance(30,KeyOk);inputNesBegin();assert(inputNesPad()==0);
  assert(inputNesPad()==1);
  advance(30,0);assert(inputNesPad()==0);advance(30,KeyOk);assert(inputNesPad()==1);
  reset();for(int i=0;i<10;++i){advance(4,KeyOk);advance(4,0);}advance(30,0);
  inputUpdate(testMillis);assert(!bOk.pressed()&&inputNesPad()==0);
  puts("PASS: real sampling code retains a 50ms tap across a 200ms UI stall, continuous movement, START/SELECT overlap, neutral launch frame without release blocking and bounce rejection");
}
