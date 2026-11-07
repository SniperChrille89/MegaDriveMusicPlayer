#include "hardware/wav_render.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc,char**argv){
 if(argc!=3){std::cerr<<"Usage: mda_vgm_to_wav input.vgm output.wav\n";return 2;}
 std::ifstream f(argv[1],std::ios::binary);
 if(!f){std::cerr<<"Cannot read VGM\n";return 2;}
 std::vector<uint8_t> in((std::istreambuf_iterator<char>(f)),{});
 std::string error; mda::WavStatistics stats;
 if(!mda::renderVgmToWav(in,argv[2],error,&stats)){std::cerr<<error<<"\n";return 1;}
 std::cout<<"WAV generated: "<<argv[2]<<" ("<<stats.frames<<" frames, peak "<<stats.peak<<", RMS "<<stats.rms<<")\n";
 return 0;
}
