#include "wav_render.h"
#include "engine.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>
namespace mda {
static void put16(std::ofstream& f, uint16_t v) { char b[2]={char(v),char(v>>8)};f.write(b,2); }
static void put32(std::ofstream& f, uint32_t v) { char b[4]={char(v),char(v>>8),char(v>>16),char(v>>24)};f.write(b,4); }
bool renderVgmToWav(const std::vector<uint8_t>& vgm, const std::string& path, std::string& error, WavStatistics* stats) {
 if(vgm.size()<0x40 || vgm[0]!='V'||vgm[1]!='g'||vgm[2]!='m'||vgm[3]!=' ') {error="Invalid VGM header";return false;}
 const auto get32=[&](size_t p)->uint32_t{return uint32_t(vgm[p])|(uint32_t(vgm[p+1])<<8)|(uint32_t(vgm[p+2])<<16)|(uint32_t(vgm[p+3])<<24);};
 uint32_t frames=get32(0x18);
 if(frames==0 || frames>44100*600U) {error="Invalid or excessively long VGM sample count";return false;}
 MegaDriveEngine engine;
 if(!engine.load(vgm,error))return false;
 // Preserve event timing and chip clocks as recorded; do not apply Sonic PAL speed profiles.
 engine.setTimingProfile(0);
 engine.setVolumes(75,120,110,100);
 std::ofstream f(path,std::ios::binary);
 if(!f){error="Cannot open WAV output";return false;}
 f.write("RIFF",4);put32(f,36+frames*4);f.write("WAVEfmt ",8);put32(f,16);put16(f,1);put16(f,2);
 put32(f,MegaDriveEngine::Rate);put32(f,MegaDriveEngine::Rate*4);put16(f,4);put16(f,16);
 f.write("data",4);put32(f,frames*4);
 std::vector<int16_t> block(4096*2);
 uint64_t done=0; long double squares=0;int peak=0;
 while(done<frames) {
  const int n=int(std::min<uint64_t>(4096,frames-done));
  engine.render(block.data(),n);
  for(int j=0;j<n*2;++j) {int s=block[j];peak=std::max(peak,std::abs(s));squares+=static_cast<long double>(s)*s;}
  f.write(reinterpret_cast<const char*>(block.data()),n*4);done+=n;
 }
 if(!f){error="WAV write failed";return false;}
 if(stats){stats->frames=done;stats->peak=peak;stats->rms=std::sqrt(double(squares/(done*2)));}
 return true;
}
}
