#include "hardware/audio_bus.h"
#include <cassert>
#include <iostream>
int main(){
    mda::SoundBus b;
    int observed=0;
    b.setSink([&](const mda::SoundWrite&){ ++observed; });
    const uint8_t code[]={0x3e,0x22,0x32,0x00};
    b.loadZ80(code,sizeof code);assert(b.z80Read(0)==0x3e);
    b.z80Write(0x2000,0x77);assert(b.z80Read(0)==0x77); // 8K mirror
    b.setCycle(48);
    b.z80Write(0x4000,0x28);
    b.z80Write(0x4001,0x7f);
    b.m68kWrite8(0xc00011,0x90);
    assert(observed==3);
    assert(b.trace()[0].chip==mda::Chip::YM2612);
    assert(b.trace()[0].z80Cycle==48 && b.trace()[0].port==0);
    assert(b.trace()[1].port==1);
    assert(b.trace()[2].chip==mda::Chip::PSG);
    b.m68kWrite8(0xa04002,0x40); assert(b.trace().back().port==2);
    b.reset();assert(b.trace().empty());assert(b.cycle()==0);
    std::cout << "Audio bus smoke test OK\n";
}
