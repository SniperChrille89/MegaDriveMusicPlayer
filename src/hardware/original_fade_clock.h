#pragma once
#include <algorithm>
#include <cstdint>

namespace mda {
// Timing-only adapter for the ORIGINAL Sonic 3 & Knuckles Z80 fade ($E1).
// No YM2612/PSG/DAC writes or PCM gain adjustments are performed here.
// The Z80 driver implements its own 40-step FM attenuation and PSG/DAC stops.
// zFadeOutTimeout ($1C0D) counts steps, zFadeOutDelay ($1C0F) counts frames.
// The adapter gates the driver's delay register so the ORIGINAL attenuation
// steps are distributed over an exact PAL/NTSC wall-clock duration.
class OriginalFadeClock {
public:
    static constexpr uint8_t Steps=40;
    static constexpr uint16_t TimeoutAddress=0x1c0d;
    static constexpr uint16_t DelayAddress=0x1c0f;

    void begin(unsigned frames) noexcept {
        totalFrames_=std::max(1u,frames);
        elapsedFrames_=0;
        observed_=false;
        finished_=false;
    }
    void cancel() noexcept { totalFrames_=elapsedFrames_=0;observed_=finished_=false; }
    bool active() const noexcept { return totalFrames_!=0; }
    bool observed() const noexcept { return observed_; }
    bool finished() const noexcept { return finished_; }
    unsigned elapsedFrames() const noexcept { return elapsedFrames_; }
    unsigned totalFrames() const noexcept { return totalFrames_; }

    // Call before the Z80 receives each video interrupt. With delay=2, the
    // original routine decrements to 1 and waits; with delay=1 it executes
    // precisely one original FM fade step. Do not alter $1C0D, FM TL or PSG.
    template<class Ram>
    void prepare(Ram& bus) noexcept {
        if(!active() || !observed_ || finished_) return;
        if(elapsedFrames_ < totalFrames_) ++elapsedFrames_;
        const auto remaining=bus.z80RamRead(TimeoutAddress);
        if(!remaining) return;
        const unsigned done=Steps-std::min<unsigned>(remaining,Steps);
        const unsigned wanted=std::min<unsigned>(Steps,
            (uint64_t(elapsedFrames_)*Steps)/totalFrames_);
        bus.z80Write(DelayAddress, done<wanted ? uint8_t(1) : uint8_t(2));
    }
    template<class Ram>
    void observe(Ram& bus) noexcept {
        if(!active() || finished_) return;
        const auto remaining=bus.z80RamRead(TimeoutAddress);
        if(!observed_) {
            if(remaining==Steps) {
                observed_=true;
                // The game's $E1 initialization has just run. Count this
                // frame as the first frame in the requested duration.
                elapsedFrames_=1;
            }
            return;
        }
        if(remaining==0 && elapsedFrames_>=totalFrames_) finished_=true;
    }
private:
    unsigned totalFrames_=0,elapsedFrames_=0;
    bool observed_=false,finished_=false;
};
}
