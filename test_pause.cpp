#include "src/engine.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc, char** argv){
    if(argc<2){std::cerr<<"Usage: test_pause file.vgm\n";return 1;}
    std::ifstream f(argv[1],std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
    MegaDriveEngine e;std::string error;
    if(!e.load(bytes,error)){std::cerr<<error<<"\n";return 2;}
    e.setVolumes(100,160,100,100);
    int16_t out[2];
    for(int i=0;i<44100*3;i++)e.render(out,1);
    const auto pos=e.position();
    e.requestPause();
    assert(e.isPaused());
    assert(e.isPauseTailActive());
    long long tailEnergy=0;
    for(int i=0;i<22050;i++){
        e.render(out,1);
        tailEnergy+=std::abs(int(out[0]))+std::abs(int(out[1]));
        if(e.position()!=pos){std::cerr<<"VGM time advanced during pause";return 3;}
    }
    assert(!e.isPauseTailActive());
    for(int i=0;i<100;i++){
        e.render(out,1);
        if(out[0]!=0 || out[1]!=0){std::cerr<<"Audio not silent after tail";return 4;}
    }
    e.resume();
    assert(!e.isPaused());
    e.render(out,1);
    assert(e.position()>pos);
    std::cout<<"pause_tail_frames=22050 vgm_frozen=1 final_silent=1 resume=1 tail_energy="<<tailEnergy<<"\n";
    return 0;
}
