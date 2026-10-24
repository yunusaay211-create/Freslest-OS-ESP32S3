#include "../OS/freslest/screen_mirror_codec.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace freslest_screen;
int main(){
 for(unsigned v=0;v<65536;v++)assert(rgb332(v)==(((v>>13)<<5)|(((v>>8)&7)<<2)|((v>>3)&3)));
 std::vector<uint8_t> out(maxPacket+16,0xac);
 std::vector<uint16_t> source(480*360);
 assert(!encode(out.data(),23,source.data(),480,360,1,true,false,0,0,0));
 assert(!encode(out.data(),maxPacket,source.data(),480,360,0,true,false,0,0,0));
 for(unsigned test=0;test<4;test++){
  unsigned h=test&1?360:320,div=test>=2?2:1;
  for(unsigned i=0;i<480*h;i++){unsigned v=test==0?0:((i*73u)^(i>>3)^0x5a5a)&65535;source[i]=(v>>8)|(v<<8);}
  size_t n=encode(out.data(),maxPacket,source.data(),480,h,div,true,false,0,0,test+1);assert(n&&n<=maxPacket);
  for(unsigned i=maxPacket;i<out.size();i++)assert(out[i]==0xac);
  assert(fwrite(out.data(),1,n,stdout)==n);
 }
 std::fill(source.begin(),source.end(),0xffff);
 size_t n=encode(out.data(),maxPacket,source.data(),480,320,1,true,true,4,4,99);assert(n);
 assert(fwrite(out.data(),1,n,stdout)==n);
}
