#include "engine.h"
#include <vector>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <algorithm>
static void p32(std::vector<uint8_t>& b,size_t p,uint32_t n){for(int i=0;i<4;i++)b[p+i]=uint8_t(n>>(i*8));}
int main(){
    std::vector<uint8_t> v(0x40);v[0]='V';v[1]='g';v[2]='m';v[3]=' ';
    p32(v,8,0x150);p32(v,0x34,0x0c);p32(v,0x0c,3579545);p32(v,0x2c,7670454);
    // Programmable PSG noise, a sustained FM voice and DAC samples;
    // ensure the stem reconstruction adds up to the established v0.25 mixer.
    v.insert(v.end(),{
      0x50,0x80,0x50,0x20,0x50,0x90,
      0x52,0xb0,0x32,0x52,0xb4,0xc0,
      0x52,0x30,0x71,0x52,0x34,0x01,0x52,0x38,0x01,0x52,0x3c,0x01,
      0x52,0x40,0x14,0x52,0x44,0x14,0x52,0x48,0x14,0x52,0x4c,0x14,
      0x52,0x50,0x1f,0x52,0x54,0x1f,0x52,0x58,0x1f,0x52,0x5c,0x1f,
      0x52,0xa0,0x98,0x52,0xa4,0x22,0x52,0x28,0xf0,
      0x52,0x2b,0x80
    });
    for(int j=0;j<150;j++){
       v.insert(v.end(),{0x52,0x2a,uint8_t(j%2?0xff:0x00),0x61,0x32,0x00});
    }
    v.push_back(0x66);p32(v,4,uint32_t(v.size()-4));
    MegaDriveEngine reference,split;std::string err;
    if(!reference.load(v,err)||!split.load(v,err)){std::cerr<<err;return 1;}
    reference.setVolumes(75,120,110,100);split.setVolumes(75,120,110,100);
    const int frames=6800;
    std::vector<int16_t> mix(frames*2),stems(frames*14);
    reference.render(mix.data(),frames);split.renderStems(stems.data(),frames);
    int maxAbs=0;double sumSq=0;
    for(int i=0;i<frames;i++)for(int side=0;side<2;side++){
        int summed=0;
        for(int channel=0;channel<7;channel++)summed+=stems[i*14+channel*2+side];
        const int difference=std::abs(summed-int(mix[i*2+side]));
        maxAbs=std::max(maxAbs,difference);sumSq+=double(difference)*difference;
    }
    std::cout<<"Stem reconstruction maximum sample difference="<<maxAbs<<" RMS="<<sqrt(sumSq/(frames*2))<<"\n";
    // Rounding at most 7 channels introduces a few integer PCM LSBs.
    return maxAbs<=9?0:2;
}
