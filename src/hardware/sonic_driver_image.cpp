#include "sonic_driver_image.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace mda {
KosinskiResult decompressKosinski(const std::vector<uint8_t>& rom, size_t start, size_t limit) {
 KosinskiResult res;
 if (start >= rom.size() || limit == 0) { res.error="Invalid input"; return res; }
 size_t pos=start;
 uint16_t descriptor=0;
 unsigned bits=0;
 auto readByte=[&]() -> uint8_t { if(pos>=rom.size()) throw std::runtime_error("Truncated stream");return rom[pos++]; };
 auto refill=[&]() { uint8_t lo=readByte(),hi=readByte();descriptor=uint16_t(lo | (uint16_t(hi)<<8));bits=16; };
 // The 68k Kosinski routine prefetches a new 16-bit descriptor BEFORE the
 // literal/back-reference bytes of the 16th operation. Refill-on-next-bit is wrong.
 try {
  refill();
  auto nextBit=[&]() -> unsigned {
    unsigned v=descriptor&1;
    descriptor>>=1;
    if(--bits==0) refill();
    return v;
  };
  while(res.output.size()<limit) {
   if(nextBit()) {res.output.push_back(readByte());continue;}
   int distance=0; unsigned length=0;
   if(!nextBit()) {
    unsigned count=(nextBit()<<1)|nextBit();
    distance=int(readByte())-256;
    length=count+2;
   } else {
    uint8_t lo=readByte(),hi=readByte();
    distance=-8192+((hi&0xf8)<<5)+lo;
    unsigned n=hi&7;
    if(n) length=n+2;
    else {
     n=readByte();
     if(n==0){res.ok=true;res.bytesConsumed=pos-start;return res;}
     if(n==1) continue;
     length=n+1;
    }
   }
   if(distance>=0 || size_t(-distance)>res.output.size() || length>limit-res.output.size())
     throw std::runtime_error("Invalid Kosinski reference or output overflow");
   for(unsigned j=0;j<length;++j) res.output.push_back(res.output[res.output.size()+distance]);
  }
  throw std::runtime_error("Kosinski output exceeds Z80 RAM region");
 } catch(const std::exception& e) {res.error=e.what();res.output.clear();return res;}
}

static uint32_t be32(const std::vector<uint8_t>& b,size_t p) {
 return (uint32_t(b[p])<<24)|(uint32_t(b[p+1])<<16)|(uint32_t(b[p+2])<<8)|uint32_t(b[p+3]);
}
// Find LEA source,a0; LEA Z80 RAM address,a1 sequences in Sonic3K's
// 68000 SndDrvInit. Search actual ROM pointers, not a chance Z80 signature
// inside a compressed byte stream (the bug in v0.19).
static bool findCopy(const std::vector<uint8_t>& rom,uint32_t dest,uint32_t& source){
 if(rom.size()<16) return false;
 const size_t end=std::min<size_t>(rom.size()-12,0x20000);
 for(size_t i=0;i<end;++i) {
  if(rom[i]!=0x41 || rom[i+1]!=0xF9 || rom[i+6]!=0x43 || rom[i+7]!=0xF9) continue;
  if(be32(rom,i+8)!=0xA00000U+dest) continue;
  uint32_t ptr=be32(rom,i+2);
  if(ptr>=rom.size()) continue;
  source=ptr;return true;
 }
 return false;
}
SonicDriverImage findSonic3KZ80Bootstrap(const std::vector<uint8_t>& rom) {
 SonicDriverImage out;
 uint32_t code=0,data=0,defaults=0;
 if(!findCopy(rom,0x0000,code) || !findCopy(rom,0x1300,data) || !findCopy(rom,0x1C00,defaults)) {
  out.message="Sonic 3K 68000 sound-init pointers not identified; unsupported ROM revision";
  return out;
 }
 auto c=decompressKosinski(rom,code,0x1300);
 auto d=decompressKosinski(rom,data,0x900);
 if(!c.ok || !d.ok) {out.message="Kosinski decompression failed (driver: "+c.error+", data: "+d.error+")";return out;}
 if(c.output.size()<0x90 || c.output[0]!=0xf3 || c.output[1]!=0xf3 || c.output[2]!=0xed || c.output[3]!=0x56 || c.output[4]!=0xc3) {
  out.message="Decoded sound program does not have expected Z80 reset header";return out;
 }
 uint16_t entry=uint16_t(c.output[5]|(uint16_t(c.output[6])<<8));
 if(entry+3>c.output.size() || c.output[entry]!=0x31 || c.output[entry+1]!=0x00 || c.output[entry+2]!=0x20){
  out.message="Decoded Z80 entry does not start with LD SP,$2000";return out;
 }
 if(defaults+16>rom.size()) {out.message="Truncated ROM default variables";return out;}
 out.initialRam.assign(8192,0);
 std::copy(c.output.begin(),c.output.end(),out.initialRam.begin());
 std::copy(d.output.begin(),d.output.end(),out.initialRam.begin()+0x1300);
 // 68000 SndDrvInit copies 16 default bytes to zDataStart at $1C00.
 std::copy(rom.begin()+defaults,rom.begin()+defaults+16,out.initialRam.begin()+0x1c00);
 out.found=true;out.romOffset=code;out.dataRomOffset=data;
 out.message="Kosinski Z80 program and secondary sound tables decoded; game IRQ/music command still required";
 return out;
}
}
