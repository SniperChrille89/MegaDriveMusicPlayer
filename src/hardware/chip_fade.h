#pragma once
#include "audio_bus.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mda {
// A deterministic *chip-register* fade, not a PCM master gain.
// It works on one continuous Z80 trace and never alters the game's RAM.
// Only FM Total Level values are attenuated before Nuked's YM2612
// output. DAC channel 6 and all four PSG voices are muted immediately,
// matching the requested Sonic 3K-style fade behavior.
class ChipFade {
public:
    void begin(unsigned frames) noexcept {
        // Restarting a fade must not suddenly restore the old song's volume.
        if(active_ && total_ && elapsed_ < total_)
            startGain_ *= 1.0 - double(elapsed_)/double(total_);
        else
            startGain_ = 1.0;
        total_ = std::max(1u, frames);
        elapsed_ = 0;
        active_ = true;
        pendingDACMute_ = true;
    }
    void cancel() noexcept { active_ = false; elapsed_ = total_ = 0; startGain_=1.0; pendingDACMute_=false; }
    bool active() const noexcept { return active_; }
    bool finished() const noexcept { return active_ && elapsed_ >= total_; }
    unsigned remaining() const noexcept { return active_ && elapsed_ < total_ ? total_-elapsed_ : 0; }
    unsigned elapsed() const noexcept { return elapsed_; }

    // Input is the raw writes emitted by the *same* live Z80 session.
    // The output may add register refreshes at the video-frame boundary.
    std::vector<SoundWrite> frame(const std::vector<SoundWrite>& in, uint64_t endCycle) {
        std::vector<SoundWrite> out;
        out.reserve(in.size() + (active_ ? 120 : 0));
        if(active_ && elapsed_ < total_) ++elapsed_;
        const double p = active_ ? 1.0 - startGain_*(1.0-double(elapsed_)/double(total_)) : 0.0;
        for(const auto& w : in) {
            SoundWrite v=w;
            if(w.chip==Chip::YM2612) {
                const int port=w.port & 3, side=port>>1;
                if(!(port&1)) address_[side]=w.value;
                else {
                    const uint8_t reg=address_[side];
                    if(isTL(reg)) {
                        originalTL_[side][reg]=w.value & 127;
                        knownTL_[side][reg]=true;
                        if(active_) v.value=fadeTL(w.value,p);
                    } else if(side==0 && reg==0x2a && active_) {
                        v.value=0x80; // YM2612 DAC midpoint: digital silence
                    }
                }
            } else if(w.chip==Chip::PSG) {
                if(w.value & 0x80) {
                    psgLatch_ = (w.value>>4)&7;
                    lastRawPSGLatch_=w.value;
                    hasPSGLatch_=true;
                    if(psgLatch_&1) {
                        const unsigned ch=psgLatch_>>1;
                        psgVolume_[ch]=w.value&15;
                        if(active_) v.value=(w.value&0xf0)|0x0f; // PSG volume: mute
                    }
                } else if(psgLatch_&1) {
                    const unsigned ch=psgLatch_>>1;
                    psgVolume_[ch]=w.value&15;
                    if(active_) v.value=(w.value&0xf0)|0x0f; // PSG volume: mute
                }
            }
            out.push_back(v);
        }
        if(active_) {
            // The driver is not required to rewrite static TL/PSG values during
            // a sustained note. Reapply their new attenuations every frame.
            for(int side=0;side<2;++side) for(unsigned reg=0x40;reg<=0x4f;++reg) {
                if(!knownTL_[side][reg] || !isTL(reg)) continue;
                out.push_back({Chip::YM2612,endCycle,uint16_t(side*2),uint8_t(reg)});
                out.push_back({Chip::YM2612,endCycle,uint16_t(side*2+1),fadeTL(originalTL_[side][reg],p)});
            }
            // PSG volume refresh uses the SN76489 latch protocol. Preserve
            // the most recently selected *raw* latch afterwards so that a
            // later unlatched data write is decoded just as it was by Z80.
            for(unsigned ch=0;ch<4;++ch) {
                out.push_back({Chip::PSG,endCycle,0,uint8_t(0x90 | (ch<<5) | 0x0f)});
            }
            // DAC may hold its last sample value even when the original driver
            // sends no new writes. Force it to its silent midpoint on the
            // FIRST fade frame; all later 0x2A writes are suppressed above.
            // Restore the driver's raw YM address latch immediately below.
            if(pendingDACMute_) {
                out.push_back({Chip::YM2612,endCycle,0,0x2a});
                out.push_back({Chip::YM2612,endCycle,1,0x80});
                pendingDACMute_=false;
            }
            // An address-only YM write is VGM-invisible, but our converter
            // uses it to associate subsequent DAC/data-only writes with the
            // correct register. Without this, a repeating DAC stream can be
            // mistaken for FM TL data after a synthetic refresh.
            for(int side=0;side<2;++side)
                out.push_back({Chip::YM2612,endCycle,uint16_t(side*2),address_[side]});
            // Restore the PSG latch when safe. Re-latching the noise register
            // would restart its LFSR, so don't do that for latch index 6.
            if(psgLatch_!=6) {
                if(psgLatch_&1) {
                    const auto ch=unsigned(psgLatch_>>1);
                    out.push_back({Chip::PSG,endCycle,0,uint8_t(0x80 | (psgLatch_<<4) | 0x0f)});
                } else if(psgLatch_<6 && hasPSGLatch_) {
                    out.push_back({Chip::PSG,endCycle,0,lastRawPSGLatch_});
                }
            }
        }
        return out;
    }
private:
    static bool isTL(unsigned reg) noexcept {
        // Channel 3 slots 0x?3 and 0x?7 etc are unused.
        return reg>=0x40 && reg<=0x4f && (reg&3)!=3;
    }
    static uint8_t fadeTL(unsigned raw, double p) noexcept {
        // TL is a logarithmic attenuation register (~0.75 dB per step).
        // A linear interpolation of TL values would sound almost silent by
        // mid-fade. Convert the requested amplitude envelope into dB steps.
        const int tl=raw & 127;
        if(p>=1.0) return 127;
        const double db=-20.0*std::log10(std::max(1e-8,1.0-p));
        return uint8_t(std::clamp(tl+int(std::lround(db/0.75)),0,127));
    }
    uint8_t address_[2]{};
    std::array<std::array<uint8_t,256>,2> originalTL_{};
    std::array<std::array<bool,256>,2> knownTL_{};
    std::array<uint8_t,4> psgVolume_{{15,15,15,15}};
    uint8_t psgLatch_=0;
    uint8_t lastRawPSGLatch_=0;
    bool hasPSGLatch_=false;
    bool active_=false;
    bool pendingDACMute_=false;
    double startGain_=1.0;
    unsigned total_=0,elapsed_=0;
};
}
