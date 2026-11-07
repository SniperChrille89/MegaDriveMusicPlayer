#include "hardware/sonic_driver_image.h"
#include "hardware/z80_driver.h"
#include "hardware/vgm_trace.h"
#include "hardware/wav_render.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <algorithm>
int main(int argc,char** argv){
 if(argc<2){std::cerr<<"Usage: mda_s3k_z80_probe Sonic3K.bin [output.vgm] [music_id_hex] [output.wav]\n";return 2;}
 std::ifstream input(argv[1],std::ios::binary);
 if(!input){std::cerr<<"Cannot open ROM\n";return 2;}
 std::vector<uint8_t> rom((std::istreambuf_iterator<char>(input)),{});
 auto image=mda::findSonic3KZ80Bootstrap(rom);
 if(!image.found){std::cerr<<image.message<<"\n";return 2;}
 std::cout<<"Decompressed Z80 program ROM offset: 0x"<<std::hex<<image.romOffset
          <<"; sound tables at: 0x"<<std::hex<<image.dataRomOffset<<std::dec<<"; RAM image bytes: "<<std::dec<<image.initialRam.size()<<"\n";
 mda::SoundBus bus;
 bus.loadRom(rom.data(),rom.size());
 mda::Z80Driver cpu(bus);
 cpu.load(image.initialRam.data(),image.initialRam.size());
 // cpu.load resets the bus; restore cartridge backing afterwards.
 bus.loadRom(rom.data(),rom.size());
 // Execute the REAL decompressed Sonic 3K driver code. The 68000 in the
 // original game first decompresses two chunks and populates Z80 RAM, now
 // reproduced by findSonic3KZ80Bootstrap.
 uint64_t executed=cpu.runCycles(500000);
 std::cout<<"Driver boot PC=0x"<<std::hex<<cpu.pc()<<std::dec
          <<"; YM writes="<<bus.trace().size()<<" (all chips)\n";
 // The Z80 driver's zMusicNumber input is at $1C0A (skdisasm).
 // This emulates the 68k posting a music request, not full game emulation.
 unsigned music=1;
 if(argc>=4) {
   try { music=std::stoul(argv[3],nullptr,16); }
   catch(...) {std::cerr<<"Invalid hexadecimal music ID\n";return 2;}
   if(music>0xff) {std::cerr<<"Music ID out of range\n";return 2;}
 }
 bus.z80Write(0x1c0a, static_cast<uint8_t>(music));
 std::cout<<"Posted original Z80 music command $"<<std::hex<<music<<std::dec<<" to $1C0A\n";
 // Sonic's Z80 gets V-blank interrupts (PAL = 50, NTSC = 60).
 // Default this probe to PAL as in the user's game console; this is still
 // an instruction-level approximation (not a bus-cycle accurate MD emulator).
 bus.z80Write(0x1c02,1); // zPalFlag
 constexpr unsigned frames=300; // 6 seconds at PAL for an initial listening test
 constexpr uint64_t clockHz=3546895; // PAL Z80 master/15, approximate
 constexpr unsigned frameHz=50;
 for(unsigned frame=0;frame<frames;++frame) {
    cpu.requestInterrupt();
    executed += cpu.runCycles(clockHz/frameHz);
 }

 size_t fm=0,psg=0;
 for(const auto& w:bus.trace()) (w.chip==mda::Chip::YM2612 ? fm : psg)++;
 std::cout<<"Z80 cycles: "<<executed<<"; PC=0x"<<std::hex<<cpu.pc()<<std::dec
          <<"; bank=0x"<<std::hex<<bus.bank()<<std::dec<<"; YM port writes: "<<fm<<"; PSG writes: "<<psg<<"\n";
 for(size_t k=0;k<std::min<size_t>(bus.trace().size(),8);++k){ const auto& w=bus.trace()[k]; std::cout<<"write "<<k<<" chip="<<(w.chip==mda::Chip::YM2612?"YM":"PSG")<<" port="<<w.port<<" data="<<int(w.value)<<" cyc="<<w.z80Cycle<<"\n"; }
 if(argc>=3){
  auto vgm=mda::traceToVgm(bus.trace(),cpu.cycles(),clockHz,7600489,clockHz);
  if(!mda::writeVgm(argv[2],vgm)){std::cerr<<"Cannot write VGM\n";return 2;}
  std::cout<<"Diagnostic trace: "<<argv[2]<<"\n";
  if(argc>=5){
   std::string error; mda::WavStatistics stats;
   if(!mda::renderVgmToWav(vgm,argv[4],error,&stats)){
    std::cerr<<"WAV rendering failed: "<<error<<"\n";return 1;
   }
   std::cout<<"Rendered WAV: "<<argv[4]<<" ("<<stats.frames<<" frames, peak "<<stats.peak<<", RMS "<<stats.rms<<")\n";
  }
 }
 std::cout<<"NOTE: Diagnostic ROM-driven audio. Track identity and console-accurate timing remain unverified.\n";
 return 0;
}
