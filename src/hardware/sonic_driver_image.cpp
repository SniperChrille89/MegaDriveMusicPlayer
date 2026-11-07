#include "sonic_driver_image.h"
#include <array>
#include <algorithm>
namespace mda {
SonicDriverImage findSonic3KZ80Bootstrap(const std::vector<uint8_t>& rom) {
  SonicDriverImage out;
  constexpr std::array<uint8_t,5> signature{{0xf3,0xf3,0xed,0x56,0xc3}};
  // First five bytes: DI; DI; IM 1; JP zInitAudioDriver.
  if (rom.size()<0x2000) {out.message="ROM too small";return out;}
  auto it=std::search(rom.begin(),rom.end(),signature.begin(),signature.end());
  if(it==rom.end()){out.message="Sonic 3K Z80 bootstrap signature not found";return out;}
  size_t pos=size_t(it-rom.begin());
  if(pos+7>rom.size()) {out.message="Truncated Z80 bootstrap";return out;}
  uint16_t target=uint16_t(rom[pos+5] | (uint16_t(rom[pos+6])<<8));
  if(target >= 0x1b00 || target < 0x38){out.message="Implausible Z80 entry jump target";return out;}
  out.found=true;
  out.romOffset=uint32_t(pos);
  // The Z80 only has 8KB RAM; copy initial program bytes, NOT 68k ROM data.
  const size_t count=std::min<size_t>(0x1bf0,rom.size()-pos);
  out.initialRam.assign(rom.begin()+pos, rom.begin()+pos+count);
  out.message="Candidate bootstrap found; CPU initialization and IRQs still need verification";
  return out;
}
}
