#include "src/engine.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdint>
#include <cstdlib>
int main(int argc,char**argv){
 if(argc<2)return 2;
 std::ifstream f(argv[1],std::ios::binary);
 std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
 int64_t sums[3]{};
 for(int mode=0;mode<3;mode++){
   MegaDriveEngine e;std::string err;if(!e.load(bytes,err)){std::cerr<<err;return 3;}
   e.setVolumes(100,100,100,100);
   e.setYamahaCombined(mode==1);
   e.setAnalogLowpass(mode==2,55);
   std::vector<int16_t> frame(44100*2);
   e.render(frame.data(),44100);
   for(int16_t v:frame)sums[mode]+=std::abs((int)v);
   if(!sums[mode])return 4;
 }
 if(sums[0]!=sums[1]) {std::cerr<<"Combined Yamaha path must equal separate 100% FM/DAC";return 5;}
 std::cout<<"PASS: combined YM unity matches separate buses; lowpass renders. sums="<<sums[0]<<","<<sums[1]<<","<<sums[2]<<"\n";
}
