#include "hardware/chip_fade.h"
#include <cassert>
#include <iostream>
using namespace mda;
static uint8_t last(const std::vector<SoundWrite>& v,Chip chip,unsigned port){
    for(auto it=v.rbegin();it!=v.rend();++it)
        if(it->chip==chip && it->port==port)return it->value;
    return 0xff;
}
int main(){
    ChipFade fade;
    std::vector<SoundWrite> in{
        {Chip::YM2612,100,0,0x40}, {Chip::YM2612,101,1,0x10},
        {Chip::YM2612,102,0,0x2a}, {Chip::YM2612,103,1,0xe0},
        {Chip::PSG,104,0,0x90}
    };
    auto base=fade.frame(in,110);
    assert(base.size()==in.size());
    fade.begin(150); // PAL 50 Hz * 3s
    // No DAC writes from the game: still force immediate DAC silence.
    const auto first=fade.frame({},111);
    bool forcedDac=false;
    for(size_t i=1;i<first.size();++i)
        if(first[i-1].chip==Chip::YM2612 && first[i-1].port==0 && first[i-1].value==0x2a &&
           first[i].chip==Chip::YM2612 && first[i].port==1 && first[i].value==0x80)
            forcedDac=true;
    assert(forcedDac);
    for(unsigned ch=0;ch<4;++ch) {
        bool muted=false;
        for(const auto& w:first)
            if(w.chip==Chip::PSG && w.value==uint8_t(0x9f+(ch<<5))) muted=true;
        assert(muted);
    }
    for(unsigned i=1;i<75;i++) {
        auto w=fade.frame({},200+i);
        assert(!w.empty());assert(fade.remaining()==149-i);
    }
    // A sustained FM level is updated even when the driver emits no TL writes.
    const auto half=fade.frame({{Chip::YM2612,300,0,0x2a},{Chip::YM2612,301,1,0xe0}},302);
    // DAC must already be completely muted even while FM is only half faded.
    assert(half[1].chip==Chip::YM2612 && half[1].value==0x80);
    // FM TL stays audible at half-time: ~6 dB / 8 TL steps, NOT 48 dB.
    bool foundTL=false;
    for(size_t j=1;j<half.size();++j)
        if(half[j-1].chip==Chip::YM2612 && half[j-1].value==0x40 &&
           half[j].chip==Chip::YM2612 && half[j].port==1) {
            assert(half[j].value>=0x17 && half[j].value<=0x1a);
            foundTL=true;
        }
    assert(foundTL);
    // All four PSG channels remain silent after the first fade frame.
    for(unsigned ch=0;ch<4;++ch) {
        bool muted=false;
        for(const auto& w:half)
            if(w.chip==Chip::PSG && w.value==uint8_t(0x9f+(ch<<5))) muted=true;
        assert(muted);
    }
    for(unsigned i=76;i<150;i++)fade.frame({},400+i);
    assert(fade.finished());
    auto silent=fade.frame({{Chip::YM2612,1000,0,0x2a},{Chip::YM2612,1001,1,0xe0}},1002);
    // All PSG channels must have emitted a mute value.
    bool firstPSGMuted=false;for(const auto& v:silent) if(v.chip==Chip::PSG && v.value==0x9f)firstPSGMuted=true;
    assert(firstPSGMuted);
    // Long running loop without a new fade request has no influence on duration.
    fade.cancel();
    for(unsigned i=0;i<1000;i++)fade.frame({},2000+i);
    fade.begin(250); // PAL 50 Hz * 5s
    for(unsigned i=0;i<249;i++){ fade.frame({},3000+i); assert(!fade.finished()); }
    fade.frame({},3300);assert(fade.finished());
    fade.cancel();assert(!fade.active());
    // A subsequent frame of plain DAC writes must use the same YM address
    // (the synthetic TL refresh must not steal the DAC register latch).
    fade.begin(3);
    auto latch=fade.frame({{Chip::YM2612,5000,0,0x2a},
                           {Chip::YM2612,5001,1,0x90}},5002);
    bool restored=false;
    for(const auto& w:latch)
        if(w.chip==Chip::YM2612 && w.port==0 && w.value==0x2a) restored=true;
    assert(restored);
    bool mutedDac=false;
    for(size_t i=1;i<latch.size();++i)
        if(latch[i-1].chip==Chip::YM2612 && latch[i-1].port==0 && latch[i-1].value==0x2a &&
           latch[i].chip==Chip::YM2612 && latch[i].port==1 && latch[i].value==0x80) mutedDac=true;
    assert(mutedDac);
    fade.cancel();assert(!fade.active());
    // Override/new song: PSG and DAC writes must resume their raw values.
    const auto resumed=fade.frame({{Chip::YM2612,6000,0,0x2a},
                                   {Chip::YM2612,6001,1,0xe0},
                                   {Chip::PSG,6002,0,0x92}},6003);
    assert(resumed.size()==3);
    assert(resumed[1].value==0xe0);
    assert(resumed[2].value==0x92);
    std::cout<<"3s/5s FM fade plus immediate DAC/PSG mute OK\n";
}
