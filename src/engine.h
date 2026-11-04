#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <string>
#include "ym3438.h"

// Original application glue (not part of the LGPL Nuked core).
class MegaDriveEngine {
public:
    bool load(const std::vector<uint8_t>& bytes, std::string& error);
    void render(int16_t* stereo, int frames);
    void requestFade();
    void requestPause();
    void resume();
    void stop();
    bool isFinished() const {return finished_;}
    bool isPaused() const {return paused_;}
    uint64_t position() const {return samples_;}
    static constexpr int Rate = 44100;
private:
    void reset();
    void writeYM(int port,uint8_t address,uint8_t value);
    void clockYMWrite();
    void command();
    void startFade(bool pause);
    void applyFade();
    void tick();
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
    int ymWriteStage_ = 0; // 0 = address, 1 = data
    int ymWriteDelay_ = 0; // number of clocks between writes
    YMWrite ymActive_{};
    std::vector<uint8_t> vgm_, pcm_;
    size_t cursor_=0, loop_=0, pcmIndex_=0;
    uint32_t wait_=0, ymClock_=7670454, psgClock_=3579545;
    double clockFrac_=0;
    uint64_t samples_=0;
    bool finished_=true, paused_=false, fading_=false, fadePause_=false;
    int fadeStep_=0, fadeCounter_=0;
    uint8_t registers_[2][256]{};
};
