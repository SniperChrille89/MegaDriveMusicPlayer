#include "hardware/sonic_driver_image.h"
#include "hardware/z80_driver.h"
#include "hardware/vgm_trace.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <algorithm>
int main(int argc,char** argv){
 if(argc<2){std::cerr<<"Usage: mda_s3k_z80_probe Sonic3K.bin [output.vgm]\n";return 2;}
 std::ifstream input(argv[1],std::ios::binary);
 if(!input){std::cerr<<"Cannot open ROM\n";return 2;}
 std::vector<uint8_t> rom((std::istreambuf_iterator<char>(input)),{});
 auto image=mda::findSonic3KZ80Bootstrap(rom);
 if(!image.found){std::cerr<<image.message<<"\n";return 2;}
 std::cout<<"Candidate Z80 bootstrap ROM offset: 0x"<<std::hex<<image.romOffset
          <<"; loaded bytes: "<<std::dec<<image.initialRam.size()<<"\n";
 mda::SoundBus bus;
 bus.loadRom(rom.data(),rom.size());
 mda::Z80Driver cpu(bus);
 cpu.load(image.initialRam.data(),image.initialRam.size());
 // cpu.load resets the bus; restore cartridge backing afterwards.
 bus.loadRom(rom.data(),rom.size());
 // Test the original initialization program, not a synthetic melody.
 // No game 68k startup / V-int emulation is asserted by this probe.
 uint64_t executed=cpu.runCycles(500000);
 size_t fm=0,psg=0;
 for(const auto& w:bus.trace()) (w.chip==mda::Chip::YM2612 ? fm : psg)++;
 std::cout<<"Z80 cycles: "<<executed<<"; PC=0x"<<std::hex<<cpu.pc()<<std::dec
          <<"; YM port writes: "<<fm<<"; PSG writes: "<<psg<<"\n";
 if(argc>=3){
  auto vgm=mda::traceToVgm(bus.trace(),cpu.cycles());
  if(!mda::writeVgm(argv[2],vgm)){std::cerr<<"Cannot write VGM\n";return 2;}
  std::cout<<"Diagnostic trace: "<<argv[2]<<"\n";
 }
 std::cout<<"NOTE: This only checks CPU bootstrap. No claim of music playback; game interrupts and commands are not yet wired.\n";
 return 0;
}
