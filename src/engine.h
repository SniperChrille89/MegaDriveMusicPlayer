#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <array>
#include <string>
#include "ym3438.h"

// Original application glue (not part of the LGPL Nuked core).
class MegaDriveEngine {
public:
    bool load(const std::vector<uint8_t>& bytes, std::string& error);
    void render(int16_t* stereo, int frames);
    // Seven synchronous stereo stems: PSG, FM1..FM5, FM6/DAC.
    // All taps are captured from ONE Nuked YM2612 chip instance.
    void renderStems(int16_t* interleaved14, int frames);
    // Extend a ROM-backed VGM without resetting the chip, DAC or FIR state.
    // next contains VGM opcodes and a final 0x66 (no header).
    bool appendRomCommands(const std::vector<uint8_t>& next, uint64_t totalSamples);
    void enableRomStream() { romStream_=true; }
    bool romStreamStarved() const { return romStreamStarved_; }
    void requestFade();
    void requestPause();
    void resume();
    void stop();
    // Gain percentages: 0 = mute, 100 = original v0.4 level, 200 = twice that level.
    void setVolumes(int psg, int fm, int dac, int master);
    // Diagnostic: bypass separate FM/DAC gain routing and mix the entire original Nuked output.
    void setYamahaCombined(bool enabled) { yamahaCombined_ = enabled; }
    void setAnalogLowpass(bool enabled, int strengthPercent);
    // Output hardware-region setting and independent music-driver timing profile.
    // 0 = preserve recorded timing, 1 = Sonic 1-like VBlank,
    // 2 = Sonic 3K-like PAL compensation (approximate).
    void setTimingProfile(int profile);
    int timingProfile() const { return timingProfile_; }
    double eventSpeed() const;
    // Set 50 Hz (Europe) or 60 Hz (USA/Japan) hardware profile.
    void setRegion(int hz);
    int sourceRate() const { return sourceRate_; }
    bool sourceRateDeclared() const { return sourceRateDeclared_; }
    int region() const { return targetRate_; }
    bool isFinished() const {return finished_;}
    bool isPaused() const {return paused_;}
    bool isFading() const {return fading_;}
    bool isPauseTailActive() const {return paused_ && pauseTailFrames_ > 0;}
    uint64_t position() const {return samples_;}
    uint64_t initialPrimeClocks() const {return initialYMPrimeClocks_;}
    int initialPrimeWrites() const {return initialYMPrimeWrites_;}
    bool initialPrimed() const {return initialYMPrimed_;}
    static constexpr int Rate = 44100;
private:
    void reset();
    void writeYM(int port,uint8_t address,uint8_t value);
    void clockYMWrite();
    void primeInitialYMRegisters();
    void command();
    void startFade(bool pause);
    void applyFade();
    void tick();
    void tickVgm();
    uint32_t u32(size_t index) const;
    uint16_t u16(size_t index) const;
    void sample(int16_t* output);
    struct PSG {
        uint16_t tone[3]{1,1,1}; uint8_t vol[4]{15,15,15,15};
        uint8_t noise=0, latch=0;
        double phase[3]{}, noisePhase=0;
        uint16_t lfsr=0x8000;
        double clock=3579545.0;
        void write(uint8_t v);
        float render();
    } psg_;
    ym3438_t ym_{};
    struct YMWrite { uint8_t port, address, value; };
    std::deque<YMWrite> ymWrites_;
    uint64_t initialYMPrimeClocks_ = 0;
    int initialYMPrimeWrites_ = 0;
    bool initialYMPrimed_ = false;
    int ymWriteStage_ = 0; // 0 = address, 1 = data
    int ymWriteDelay_ = 0; // number of clocks between writes
    YMWrite ymActive_{};
    std::vector<uint8_t> vgm_, pcm_;
    size_t cursor_=0, loop_=0, pcmIndex_=0;
    uint32_t wait_=0, ymClock_=7670454, psgClock_=3579545;
    // Preserve every native YM2612 output slot, then low-pass at the chip
    // output rate (~1.28 MHz) before sampling at 44.1 kHz. This avoids
    // the phase-dependent 3/4-clock averaging used before v0.25.
    static constexpr int FirTaps = 385;
    std::array<std::array<float, FirTaps>, 4> reconstruct_{};
    int reconstructHead_ = 0;
    // Native YM2612 slot-separated rings, enabled only during offline export.
    std::array<std::array<float, FirTaps>, 12> stemReconstruct_{};
    bool captureStems_=false;
    float lastPsg_=0.f;
    void stemSample(int16_t* out14);
    double clockFrac_=0;
    double eventFrac_=0;
    int sourceRate_=60, targetRate_=60, timingProfile_=0;
    bool sourceRateDeclared_=false;
    uint64_t samples_=0;
    bool finished_=true, paused_=false, fading_=false;
    bool romStream_=false, romStreamStarved_=false;
    // While paused the VGM sequencer is frozen, but the YM2612 runs through
    // a short natural release envelope, as on the console.
    int pauseTailFrames_=0;
    static constexpr int PauseTailFrames = Rate / 2;
    float psgGain_=1.f, fmGain_=1.f, dacGain_=1.f, masterGain_=1.f;
    bool yamahaCombined_=false, analogLowpassEnabled_=false;
    int lowpassStrength_=45;
    float lowpassL_=0, lowpassR_=0;
    int fadeStep_=0, fadeCounter_=0;
    uint8_t registers_[2][256]{};
};
