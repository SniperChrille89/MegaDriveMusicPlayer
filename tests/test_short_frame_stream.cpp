#include "engine.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include <cstdint>
static void word(std::vector<uint8_t>& v,size_t p,uint32_t x){for(int i=0;i<4;i++)v[p+i]=uint8_t(x>>(i*8));}
static void delay(std::vector<uint8_t>& v,uint32_t n){while(n){auto c=std::min(n,65535u);v.insert(v.end(),{0x61,uint8_t(c),uint8_t(c>>8)});n-=c;}}
static void finish(std::vector<uint8_t>& v,uint32_t total){v.push_back(0x66);word(v,4,uint32_t(v.size()-4));word(v,0x18,total);}
int main(){
  const uint32_t frames=882; // one PAL frame at 44.1 kHz
  std::vector<uint8_t> first(0x40,0);std::memcpy(first.data(),"Vgm ",4);word(first,8,0x150);word(first,0x34,0xc);word(first,0xc,3546895);word(first,0x2c,7600489);
  first.insert(first.end(),{0x50,0x82,0x50,0x01,0x50,0x90});delay(first,frames);finish(first,frames);
  std::vector<uint8_t> second;delay(second,frames);second.push_back(0x66);
  std::vector<uint8_t> third;delay(third,frames);third.push_back(0x66);
  std::vector<uint8_t> all=first;all.pop_back();all.insert(all.end(),second.begin(),second.end()-1);all.insert(all.end(),third.begin(),third.end());
  word(all,4,uint32_t(all.size()-4));word(all,0x18,3*frames);
  MegaDriveEngine streamed,reference;std::string error;
  assert(streamed.load(first,error));streamed.enableRomStream();assert(reference.load(all,error));
  for(int i=0;i<3;i++){
    std::vector<int16_t> a(frames*2),b(frames*2);
    if(i==1)assert(streamed.appendRomCommands(second,2*frames));
    if(i==2)assert(streamed.appendRomCommands(third,3*frames));
    streamed.render(a.data(),frames);reference.render(b.data(),frames);
    assert(a==b);
  }
  assert(!streamed.isFinished());
  std::cout<<"20 ms PAL frame appends preserve exact PCM samples and chip state\n";
}
