#include "src/engine.h"
#include <vector>
#include <iostream>
#include <cassert>
#include <cstring>
#include <cmath>
static void put32(std::vector<uint8_t>& b,int p,uint32_t v){for(int i=0;i<4;i++)b[p+i]=(v>>(8*i))&255;}
static std::vector<uint8_t> file(int hz){std::vector<uint8_t>b(0x40,0);memcpy(b.data(),"Vgm ",4);put32(b,8,0x150);put32(b,0x24,hz);put32(b,0x2c,7670454);put32(b,0xc,3579545); for(int i=0;i<60;i++)b.push_back(0x62); b.push_back(0x66);return b;}
static int duration(int source,int target,int profile){MegaDriveEngine e;std::string error; assert(e.load(file(source),error));e.setRegion(target);e.setTimingProfile(profile);std::vector<int16_t> out(2);int n=0;while(!e.isFinished()&&n<100000){e.render(out.data(),1);n++;}return n;}
int main(){
 for(int source:{50,60})for(int target:{50,60})for(int profile:{0,1,2}){
   int n=duration(source,target,profile);double speed=source==target||profile==0?1.0:(profile==1?5.0/6.0:35.0/36.0);if(source==50&&target==60&&profile!=0)speed=1/speed;
   auto expected=44100.0/speed;
   std::cout<<"source="<<source<<" target="<<target<<" profile="<<profile<<" frames="<<n<<" expected="<<expected<<"\n";
   assert(std::abs(expected-n)<5);
 }
}
