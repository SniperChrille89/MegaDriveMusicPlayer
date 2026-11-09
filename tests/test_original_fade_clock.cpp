#include "hardware/original_fade_clock.h"
#include <array>
#include <cassert>
#include <iostream>
struct FakeZ80 {
    std::array<unsigned char,8192> ram{};
    unsigned char z80RamRead(unsigned a) const {return ram[a&8191];}
    void z80Write(unsigned a,unsigned char v){ram[a&8191]=v;}
    void executeFadeFrame(){
        if(ram[0x1c0d]==0) return;
        unsigned char& delay=ram[0x1c0f];
        if(--delay==0){
            delay=ram[0x1c0e];
            --ram[0x1c0d];
        }
    }
};
void test(unsigned frames,unsigned loops){
    FakeZ80 z;
    mda::OriginalFadeClock clock;
    // Music driver is already running; a prior loop count must not change timing.
    for(unsigned i=0;i<loops;++i) z.ram[0x1c10]=static_cast<unsigned char>(i);
    clock.begin(frames);
    z.z80Write(0x1c0d,40); z.z80Write(0x1c0e,6); z.z80Write(0x1c0f,6);
    clock.observe(z);  // The $E1 command has just been executed by the Z80.
    assert(clock.observed());
    int completedAt=-1;
    for(unsigned f=2;f<=frames+5;f++) {
        clock.prepare(z);
        z.executeFadeFrame();
        clock.observe(z);
        if(clock.finished() && completedAt<0) completedAt=int(f);
        assert(z.z80RamRead(0x1c0d)<=40);
    }
    assert(completedAt==int(frames));
    assert(z.z80RamRead(0x1c0d)==0);
    clock.cancel();
    assert(!clock.active());
}
int main(){
    for(unsigned loops:{0u,1u,4u,12u,100u}) {
        test(150,loops);test(250,loops);test(180,loops);test(300,loops);
    }
    std::cout<<"Original 40-step driver fade: 3s / 5s PAL + NTSC, all loop histories OK\n";
}
