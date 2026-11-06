#include "vgm_trace.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace mda {
static void put32(std::vector<uint8_t>& b, size_t pos, uint32_t n) {
    for (int i=0;i<4;++i) b.at(pos+i) = uint8_t(n >> (8*i));
}
static void waitFor(std::vector<uint8_t>& b, uint64_t count) {
    while (count) {
        unsigned n=(unsigned)std::min<uint64_t>(count,65535);
        b.push_back(0x61); b.push_back(uint8_t(n)); b.push_back(uint8_t(n>>8));
        count-=n;
    }
}
std::vector<uint8_t> traceToVgm(const std::vector<SoundWrite>& trace,
                                uint64_t totalCycles, uint32_t cpuClock,
                                uint32_t ymClock, uint32_t psgClock) {
    if (!cpuClock) throw std::invalid_argument("CPU clock must be positive");
    std::vector<uint8_t> out(0x40,0);
    out[0]='V';out[1]='g';out[2]='m';out[3]=' ';
    put32(out,8,0x00000150);
    put32(out,0x0c,psgClock);
    put32(out,0x2c,ymClock);
    put32(out,0x34,0x0c); // data starts at absolute 0x40
    uint64_t samples=0;
    int lastAddress[2]={-1,-1};
    // Stable order matters: address/data pairs must never be reordered.
    uint64_t lastCycle=0;
    for (const auto& w: trace) {
        if (w.z80Cycle < lastCycle) throw std::invalid_argument("Unsorted CPU write trace");
        lastCycle=w.z80Cycle;
        // Integer conversion from ABSOLUTE CPU cycles avoids per-event rounding drift.
        const uint64_t at=(w.z80Cycle*44100ULL + cpuClock/2)/cpuClock;
        if(at>samples) {waitFor(out,at-samples);samples=at;}
        if(w.chip==Chip::PSG) {out.push_back(0x50);out.push_back(w.value);continue;}
        const int port=w.port&3, side=port/2;
        if((port&1)==0) lastAddress[side]=w.value;
        else if(lastAddress[side]>=0) {
            out.push_back(uint8_t(side?0x53:0x52));
            out.push_back(uint8_t(lastAddress[side]));
            out.push_back(w.value);
        }
    }
    const uint64_t endSamples=(std::max(totalCycles,lastCycle)*44100ULL+cpuClock/2)/cpuClock;
    if(endSamples>samples) waitFor(out,endSamples-samples);
    out.push_back(0x66);
    put32(out,0x04,(uint32_t)(out.size()-4));
    put32(out,0x18,(uint32_t)endSamples);
    return out;
}
bool writeVgm(const std::string& path,const std::vector<uint8_t>& bytes) {
    std::ofstream f(path,std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    return bool(f);
}
}
