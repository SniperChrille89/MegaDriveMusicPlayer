#include "hardware/z80_driver.h"
#include "hardware/vgm_trace.h"
#include <iostream>
int main(int argc,char** argv) {
    mda::SoundBus bus;
    mda::Z80Driver cpu(bus);
    // Original tiny Z80 sound routine (not copied from any game):
    // PSG channel 0 pitch, channel 0 volume on, a timed second note,
    // then mute. Uses actual instructions and $7F11 PSG writes.
    const uint8_t program[]={
        0x3e,0x80,0x32,0x11,0x7f, // LD A,80 ; LD (7F11),A
        0x3e,0x08,0x32,0x11,0x7f, // LD A,08 ; LD (7F11),A
        0x3e,0x90,0x32,0x11,0x7f, // volume 0
        0x0e,0xff,0x06,0xff,0x10,0xfe,0x0d,0x20,0xf9, // nested delay loops
        0x3e,0x84,0x32,0x11,0x7f, // change PSG frequency
        0x0e,0xff,0x06,0xff,0x10,0xfe,0x0d,0x20,0xf9,
        0x3e,0x9f,0x32,0x11,0x7f, // mute
        0x76 // HALT
    };
    cpu.load(program,sizeof program);
    cpu.runCycles(1900000);
    const auto& writes=bus.trace();
    if(writes.size()!=5) {std::cerr<<"Expected five real Z80 PSG writes, got "<<writes.size()<<"\n";return 2;}
    const auto bytes=mda::traceToVgm(writes,cpu.cycles());
    const std::string output=argc>1?argv[1]:"Z80_CPU_Demo.vgm";
    if(!mda::writeVgm(output,bytes))return 3;
    std::cout<<"Real Z80 executed "<<cpu.cycles()<<" cycles and made "<<writes.size()<<" PSG writes; saved "<<output<<"\n";
    return 0;
}
