#include "src/engine.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
int main(int argc,char**argv){
 std::ifstream f(argv[1],std::ios::binary);
 std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),{});
 const int settings[6][4]={{100,100,100,100},{100,0,0,100},{0,100,0,100},{0,0,100,100},{0,0,0,100},{100,100,100,0}};
 for(auto& v:settings){
  MegaDriveEngine e; std::string err;if(!e.load(data,err)){std::cerr<<err;return 2;}
  e.setVolumes(v[0],v[1],v[2],v[3]);
  std::vector<int16_t> a(44100*2);double power=0;int peak=0;
  for(int sec=0;sec<12;sec++){e.render(a.data(),44100);for(auto x:a){power+=double(x)*x;peak=std::max(peak,std::abs((int)x));}}
  std::cout<<"PSG "<<v[0]<<" FM "<<v[1]<<" DAC "<<v[2]<<" Master "<<v[3]<<" : rms "<<std::sqrt(power/(44100*2*12))<<" peak "<<peak<<"\n";
  if((v[0]==0&&v[1]==0&&v[2]==0)||v[3]==0){if(peak)return 4;}
 }
}
