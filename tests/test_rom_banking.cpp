#include "hardware/audio_bus.h"
#include <vector>
#include <cstdint>
#include <iostream>

static void set_bank(mda::SoundBus& b, unsigned bank) {
    // The Z80 bank latch shifts in LSB-first bits, nine writes at $6000.
    for (int i=0;i<9;++i) b.z80Write(0x6000,(bank>>i)&1);
}
int main() {
    mda::SoundBus b;
    std::vector<uint8_t> rom(0x30000,0xff);
    rom[0x000000]=0x12;rom[0x000001]=0x34;
    rom[0x8000]=0x56;rom[0x10000]=0x78;
    rom[0x10001]=0x9a;rom[0x10002]=0xbc;
    b.loadRom(rom.data(),rom.size());
    if (b.m68kRead8(0)!=0x12 || b.z80Read(0x8001)!=0x34) return 1;
    set_bank(b,1);
    if(b.bank()!=1 || b.z80Read(0x8000)!=0x56) return 2;
    b.reset(); // Resetting CPU bus latches does NOT eject cartridge.
    set_bank(b,2);
    if(b.bank()!=2 || b.z80Read(0x8000)!=0x78 || b.z80Read(0x8001)!=0x9a) return 3;
    b.z80Write(0x0020,0xaa);
    if(b.z80Read(0x2020)!=0xaa || b.z80Read(0x0020)!=0xaa) return 4;
    if(b.z80Read(0x8002)!=0xbc || b.m68kRead8(0x10002)!=0xbc) return 5;
    if(b.z80Read(0xfffe)!=0xff) return 6;
    std::cout << "ROM banking, Z80 RAM mirror, 68000 ROM read: passed\n";
}
