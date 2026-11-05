#include "src/engine.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
int main(int argc,char**argv){std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),{}); MegaDriveEngine e;std::string err;if(!e.load(data,err)){std::cerr<<err;return 1;}int16_t pcm[2];for(int i=0;i<44100*2;i++)e.render(pcm,1);e.requestFade();int n=0,max=0;while(!e.isFinished() && n<44100*6){e.render(pcm,1);max=std::max(max,std::max(std::abs(int(pcm[0])),std::abs(int(pcm[1]))));n++;}std::cout<<"fade_samples="<<n<<" duration="<<(double)n/44100<<" peak="<<max<<" finished="<<e.isFinished()<<"\n";return (n!=44100*4)?2:0;}
