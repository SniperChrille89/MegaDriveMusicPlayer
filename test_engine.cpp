#include "src/engine.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
int main(int argc,char**argv){std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});MegaDriveEngine e;std::string err;if(!e.load(bytes,err)){std::cerr<<err;return 1;}std::vector<int16_t> a(44100*2);for(int sec=0;sec<20;sec++){e.render(a.data(),44100);double power=0;int max=0;int clips=0;for(auto x:a){max=std::max(max,std::abs((int)x));power+=(double)x*x;if(std::abs((int)x)>=32767)clips++;}std::cout<<sec+1<<" peak="<<max<<" rms="<<sqrt(power/a.size())<<" clips="<<clips<<"\n";if(e.isFinished())break;} }
