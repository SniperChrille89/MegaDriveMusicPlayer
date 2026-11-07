#include "engine.h"
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
static void put32(std::vector<uint8_t>& v,size_t i,uint32_t n) { for(int j=0;j<4;j++)v[i+j]=uint8_t(n>>(8*j)); }
static double rms(const std::vector<int16_t>& a) { double s=0; for(auto x:a)s+=double(x)*x; return std::sqrt(s/a.size()); }
int main(){
  // Synthetic sustained PSG note, with a VGM loop, checks that the PSG is not
  // cut off immediately by fade, that it decays, and that stop is reached.
  std::vector<uint8_t> v(0x40,0);
  v[0]='V';v[1]='g';v[2]='m';v[3]=' ';
  put32(v,8,0x150);put32(v,0x34,0x0c);
  put32(v,0x0c,3579545);
  v.insert(v.end(),{0x50,0x80,0x50,0x20,0x50,0x90}); // tone ch0=0x200, volume=0
  auto loop=v.size();
  v.insert(v.end(),{0x61,0xff,0xff,0x66});
  put32(v,0x1c,uint32_t(loop-0x1c));
  put32(v,4,uint32_t(v.size()-4));
  MegaDriveEngine e;std::string error;
  if(!e.load(v,error)){std::cerr<<error;return 1;}
  e.setVolumes(100,0,0,100);
  std::vector<int16_t> buf(4410*2);
  e.render(buf.data(),4410);
  auto baseline=rms(buf);
  e.requestFade();
  e.render(buf.data(),4410);
  auto early=rms(buf);
  for(int i=0;i<19;i++) e.render(buf.data(),4410);
  auto middle=rms(buf);
  for(int i=0;i<18;i++) e.render(buf.data(),4410);
  auto late=rms(buf);
  e.render(buf.data(),4410);
  e.render(buf.data(),4410);
  std::cout<<"baseline="<<baseline<<" early="<<early<<" middle="<<middle<<" late="<<late<<" finished="<<e.isFinished()<<"\n";
  if(!(baseline>100 && early>baseline*.8 && middle<baseline*.65 && middle>baseline*.25 && late<baseline*.08 && e.isFinished())) return 2;
}
