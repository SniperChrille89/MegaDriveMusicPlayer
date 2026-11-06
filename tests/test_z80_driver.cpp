#include "hardware/z80_driver.h"
#include <cassert>
#include <iostream>
int main(){
    mda::SoundBus bus;
    mda::Z80Driver cpu(bus);
    // LD A,$A4 ; LD ($4000),A ; LD A,$15 ; LD ($4001),A ; HALT
    const uint8_t code[]={0x3e,0xa4,0x32,0x00,0x40,0x3e,0x15,0x32,0x01,0x40,0x76};
    cpu.load(code,sizeof code);
    cpu.runCycles(64);
    assert(cpu.pc()>=10);
    assert(bus.trace().size()==2);
    assert(bus.trace()[0].port==0 && bus.trace()[0].value==0xa4);
    assert(bus.trace()[1].port==1 && bus.trace()[1].value==0x15);
    std::cout<<"Z80 executed YM2612 port writes OK\n";
}
