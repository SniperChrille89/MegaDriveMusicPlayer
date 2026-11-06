#include "hardware/vgm_trace.h"
#include <cassert>
#include <cstdint>
#include <iostream>
static uint32_t r32(const std::vector<uint8_t>& b,size_t i) {
    return uint32_t(b[i]) | (uint32_t(b[i+1])<<8) | (uint32_t(b[i+2])<<16) | (uint32_t(b[i+3])<<24);
}
int main() {
    mda::SoundBus bus;
    bus.setCycle(0);
    bus.z80Write(0x4000,0xa4);bus.z80Write(0x4001,0x15);
    bus.setCycle(3579545);
    bus.z80Write(0x7f11,0x9f);
    auto b=mda::traceToVgm(bus.trace(),3579545);
    assert(b[0]=='V' && b[1]=='g' && b[2]=='m');
    assert(r32(b,0x18)==44100);
    assert(b[0x40]==0x52 && b[0x41]==0xa4 && b[0x42]==0x15);
    assert(b[0x43]==0x61 && b[0x44]==0x44 && b[0x45]==0xac);
    assert(b[0x46]==0x50 && b[0x47]==0x9f && b.back()==0x66);
    std::cout<<"Cycle-stamped Z80 bus -> valid VGM timing verified\n";
}
