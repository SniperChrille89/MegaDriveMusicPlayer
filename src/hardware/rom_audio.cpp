#include "rom_audio.h"
#include "sonic_driver_image.h"
#include "fade_transition.h"
#include "z80_driver.h"
#include "vgm_trace.h"
#include <algorithm>
#include <exception>
#include <atomic>
namespace mda {
RomAudioResult renderSonic3KRom(const std::vector<uint8_t>& rom, uint8_t musicId,
                                unsigned seconds, int region) {
    RomAudioResult result;
    if (rom.empty() || seconds==0 || seconds>300 || (region!=50 && region!=60)) {
        result.error="Invalid ROM, duration, or region"; return result;
    }
    auto image=findSonic3KZ80Bootstrap(rom);
    if(!image.found) {result.error="No supported Sonic 3K sound driver found: "+image.message;return result;}
    try {
        SoundBus bus;
        Z80Driver cpu(bus);
        cpu.load(image.initialRam.data(),image.initialRam.size());
        bus.loadRom(rom.data(),rom.size());
        cpu.runCycles(500000);
        bus.z80Write(0x1c0a,musicId); // Sonic 3K zMusicNumber
        bus.z80Write(0x1c02,region==50?1:0); // original Z80 PAL flag
        // 3,546,895 Hz nominal PAL clock; NTSC clock approx. 3,579,545 Hz.
        const uint32_t cpuClock=region==50?3546895u:3579545u;
        const uint32_t ymClock=region==50?7600489u:7670454u;
        const unsigned frames=seconds*unsigned(region);
        for(unsigned frame=0;frame<frames;++frame) {
            cpu.requestInterrupt();
            // Carry fractional cycles across frames instead of discarding remainder.
            const uint64_t target=(uint64_t(frame+1)*cpuClock)/unsigned(region);
            if(cpu.cycles()<target+500000ull) cpu.runCycles(target+500000ull-cpu.cycles());
        }
        for(const auto& w: bus.trace())
            (w.chip==Chip::YM2612?result.ymWrites:result.psgWrites)++;
        if(result.ymWrites==0 && result.psgWrites==0) {
            result.error="Driver did not emit sound chip writes"; return result;
        }
        result.vgm=traceToVgm(bus.trace(),cpu.cycles(),cpuClock,ymClock,cpuClock);
    } catch(const std::exception& ex) {result.error=ex.what();}
    return result;
}
}

#include <memory>
#include <stdexcept>
namespace mda {
struct Sonic3KStream::Impl {
    SoundBus bus;
    Z80Driver cpu;
    uint32_t clock;
    uint32_t ymClock;
    int hz;
    uint64_t completedFrames=0;
    uint64_t previousSample=0;
    int lastAddress[2]={-1,-1};
    bool first=true;
    std::atomic<uint8_t> queuedCommand{0};
    std::atomic<uint8_t> nextMusic{0};
    bool fadeStarted=false;
    bool fadeCountObserved=false;
    bool fadeEnded=false;
    explicit Impl(const std::vector<uint8_t>& rom, uint8_t id, int region)
      : cpu(bus),clock(region==50?3546895u:3579545u),
        ymClock(region==50?7600489u:7670454u),hz(region) {
        if(hz!=50 && hz!=60) throw std::invalid_argument("Bad region");
        const auto image=findSonic3KZ80Bootstrap(rom);
        if(!image.found) throw std::runtime_error(image.message);
        cpu.load(image.initialRam.data(),image.initialRam.size());
        bus.loadRom(rom.data(),rom.size());
        cpu.runCycles(500000);
        bus.z80Write(0x1c0a,id);
        bus.z80Write(0x1c02,hz==50?1:0);
    }
    uint64_t sampleAt(uint64_t cycles) const {
        return (cycles*44100ULL+clock/2)/clock;
    }
    static void wait(std::vector<uint8_t>& out,uint64_t count) {
        while(count) {
            const auto n=static_cast<unsigned>(std::min<uint64_t>(count,65535));
            out.push_back(0x61);out.push_back(uint8_t(n));out.push_back(uint8_t(n>>8));
            count-=n;
        }
    }
    static void put32(std::vector<uint8_t>& v,size_t p,uint32_t val) {
        for(int i=0;i<4;i++)v[p+i]=uint8_t(val>>(8*i));
    }
    RomAudioChunk advanceFrames(unsigned frames) {
        RomAudioChunk result;
        if(frames==0 || frames>unsigned(hz)*60){result.error="Bad frame count";return result;}
        for(unsigned f=0;f<frames;f++) {
            bool switchedOnThisFrame=false;
            // Write to the actual 68K->Z80 music command mailbox.
            // Only the worker advancing this CPU ever touches bus RAM.
            const uint8_t command=queuedCommand.exchange(0);
            if(command){
                bus.z80Write(0x1c0a,command);
                if(command==0xE1 || command==0xE5){
                    fadeStarted=true;fadeCountObserved=false;fadeEnded=false;
                } else if(command != 0xE2 && command != 0xE3) {
                    // This command starts a new song in the SAME Z80 instance.
                    fadeStarted=false;fadeCountObserved=false;fadeEnded=false;
                    switchedOnThisFrame=true;
                }
            }
            if(switchedOnThisFrame)result.songSwitched=true;
            cpu.requestInterrupt();
            const uint64_t target=500000ull+(++completedFrames*uint64_t(clock))/unsigned(hz);
            if(cpu.cycles()<target)cpu.runCycles(target-cpu.cycles());
            if(fadeStarted){
                const uint8_t remaining=bus.z80RamRead(0x1c0d); // zFadeOutTimeout
                if(remaining)fadeCountObserved=true;
                // Trigger the new music while the old song still has two fade
                // steps left. The next frame executes the normal Sonic 3K
                // music mailbox command; no audio device or FM/PSG reset.
                if(shouldQueueNextSong(fadeStarted,fadeCountObserved,remaining,nextMusic.load())) {
                    const uint8_t next=nextMusic.exchange(0);
                    if(next)queuedCommand.store(next);
                }
                if(!remaining && fadeCountObserved && queuedCommand.load()==0)fadeEnded=true;
            }
        }
        result.fadeFinished=fadeEnded;
        auto &out=result.vgm;
        if(first) {
            out.assign(0x40,0);
            out[0]='V';out[1]='g';out[2]='m';out[3]=' ';
            put32(out,8,0x150);put32(out,0x0c,clock);
            put32(out,0x2c,ymClock);put32(out,0x34,0x0c);
        }
        for(const auto& w:bus.trace()) {
            const uint64_t at=sampleAt(w.z80Cycle);
            if(at>previousSample){wait(out,at-previousSample);previousSample=at;}
            if(w.chip==Chip::PSG){out.push_back(0x50);out.push_back(w.value);continue;}
            const unsigned port=w.port&3,side=port/2;
            if((port&1)==0)lastAddress[side]=w.value;
            else if(lastAddress[side]>=0){
                out.push_back(side?0x53:0x52);
                out.push_back(uint8_t(lastAddress[side]));out.push_back(w.value);
            }
        }
        bus.clearTrace();
        const uint64_t end=sampleAt(cpu.cycles());
        if(end>previousSample){wait(out,end-previousSample);previousSample=end;}
        out.push_back(0x66); // appendable stream terminator
        result.totalSamples=previousSample;
        if(first){
            put32(out,4,uint32_t(out.size()-4));
            put32(out,0x18,uint32_t(result.totalSamples));
            first=false;
        }
        return result;
    }
};
Sonic3KStream::Sonic3KStream(const std::vector<uint8_t>& rom,uint8_t id,int hz)
:impl_(std::make_unique<Impl>(rom,id,hz)){}
Sonic3KStream::~Sonic3KStream()=default;
RomAudioChunk Sonic3KStream::advance(unsigned seconds){
    if(seconds==0 || seconds>60){RomAudioChunk r;r.error="Bad chunk duration";return r;}
    return impl_->advanceFrames(seconds*unsigned(impl_->hz));
}
RomAudioChunk Sonic3KStream::advanceFrames(unsigned frames){return impl_->advanceFrames(frames);}
void Sonic3KStream::sendCommand(uint8_t command){impl_->queuedCommand.store(command);}
void Sonic3KStream::queueSongAfterFade(uint8_t id){impl_->nextMusic.store(id);}
void Sonic3KStream::cancelQueuedSong(){impl_->nextMusic.store(0);}
}
