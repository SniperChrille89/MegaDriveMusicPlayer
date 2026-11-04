#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

static constexpr float levels[16] = {1,.794328f,.630957f,.501187f,.398107f,.316228f,.251189f,.199526f,.158489f,.125893f,.1f,.079433f,.063096f,.050119f,.039811f,0};
static int clamp16(float v) { return (int)std::lround(std::max(-32768.f,std::min(32767.f,v))); }
void MegaDriveEngine::PSG::write(uint8_t v) {
    if(v&0x80) {latch=(v>>4)&7; int ch=latch>>1;
        if(latch&1) vol[ch]=v&15;
        else if(ch<3) tone[ch]=(tone[ch]&0x3f0)|(v&15);
        else {noise=v&7;lfsr=0x8000;}
    } else {int ch=latch>>1;
        if(latch&1) vol[ch]=v&15;
        else if(ch<3) tone[ch]=(tone[ch]&15)|((v&63)<<4);
    }
}
float MegaDriveEngine::PSG::render() {
    float out=0;
    for(int i=0;i<3;i++) {const int div=tone[i]?tone[i]:1;
        phase[i]+=clock/(32.0*div*MegaDriveEngine::Rate);
        phase[i]-=std::floor(phase[i]);
        out+=(phase[i]<.5?1.f:-1.f)*levels[vol[i]];
    }
    const int mode=noise&3;
    const double div=(mode==3? (tone[2]?tone[2]:1) : (16<<mode));
    noisePhase+=clock/(32.0*div*MegaDriveEngine::Rate);
    while(noisePhase>=1.0) {noisePhase-=1.0;
        const unsigned fb=(noise&4)?((lfsr^(lfsr>>3))&1):(lfsr&1);
        lfsr=(lfsr>>1)|(fb<<15);
    }
    out+=(lfsr&1?1.f:-1.f)*levels[vol[3]];
    return out*.25f;
}
uint32_t MegaDriveEngine::u32(size_t i) const {if(i+4>vgm_.size())return 0;return (uint32_t)vgm_[i]|((uint32_t)vgm_[i+1]<<8)|((uint32_t)vgm_[i+2]<<16)|((uint32_t)vgm_[i+3]<<24);}
uint16_t MegaDriveEngine::u16(size_t i) const {if(i+2>vgm_.size())return 0;return (uint16_t)(vgm_[i]|(vgm_[i+1]<<8));}
void MegaDriveEngine::reset() {
    std::memset(&ym_,0,sizeof ym_);
    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Reset(&ym_);
    psg_=PSG{}; psg_.clock=psgClock_;
    ymWrites_.clear(); ymWriteStage_=0; ymWriteDelay_=0; ymActive_={};
    std::memset(registers_,0,sizeof registers_);
    clockFrac_=0; samples_=0; wait_=0; pcm_.clear();pcmIndex_=0;
    fading_=paused_=fadePause_=false;fadeStep_=fadeCounter_=0;finished_=false;
}
bool MegaDriveEngine::load(const std::vector<uint8_t>& bytes,std::string& error) {
    finished_=true; vgm_=bytes;
    if(vgm_.size()<0x40 || std::memcmp(vgm_.data(),"Vgm ",4)!=0) {error="Not a valid uncompressed VGM file";return false;}
    const uint32_t version=u32(8), data=u32(0x34);
    cursor_= version>=0x150 ? (data?0x34ull+data:0x40ull):0x40ull;
    if(cursor_>=vgm_.size()) {error="Invalid VGM data offset";return false;}
    ymClock_=u32(0x2c)&0x3fffffff;psgClock_=u32(0x0c)&0x3fffffff;
    if(!ymClock_)ymClock_=7670454;if(!psgClock_)psgClock_=3579545;
    const uint32_t loopOffset=u32(0x1c);
    loop_=loopOffset?0x1cull+loopOffset:0;
    if(loop_>=vgm_.size())loop_=0;
    reset();return true;
}
void MegaDriveEngine::writeYM(int port,uint8_t address,uint8_t value) {
    registers_[port][address]=value;
    // TFI/VGM register data remain unmodified during ordinary playback.
    // Nuked OPN2 is clock-cycle accurate: writes must be serviced over time.
    // Sending address and data back-to-back overwrites write_data before the
    // chip samples the address, producing missing/incorrect FM instruments.
    ymWrites_.push_back({(uint8_t)port,address,value});
}
void MegaDriveEngine::clockYMWrite() {
    if (ymWriteDelay_ > 0) { --ymWriteDelay_; return; }
    if (ymWriteStage_ == 1) {
        OPN2_Write(&ym_,ymActive_.port ? 3 : 1,ymActive_.value);
        ymWriteStage_ = 0;
        // Give internal slots time to latch operator parameters before the
        // next register address replaces them (24 internal chip clocks).
        ymWriteDelay_ = 24;
    } else if (!ymWrites_.empty()) {
        ymActive_ = ymWrites_.front(); ymWrites_.pop_front();
        OPN2_Write(&ym_,ymActive_.port ? 2 : 0,ymActive_.address);
        ymWriteStage_ = 1;
        ymWriteDelay_ = 2;
    }
}
void MegaDriveEngine::startFade(bool pause) {
    if(finished_)return;
    fading_=true;fadePause_=pause;fadeStep_=0;fadeCounter_=0;
    // Kill new DAC playback and PSG immediately; let FM decay through TL changes.
    writeYM(0,0x2b,0);
    for(int ch=0;ch<4;ch++)psg_.write((uint8_t)(0x90+(ch<<5)+15));
}
void MegaDriveEngine::requestFade(){startFade(false);}
void MegaDriveEngine::requestPause(){if(!paused_&&!fading_)startFade(true);}
void MegaDriveEngine::resume(){if(paused_){paused_=false;fading_=false;fadeStep_=0;
        // Restore operator TLs, without re-keying held notes.
        for(int p=0;p<2;p++)for(int a=0x40;a<=0x4f;a++)writeYM(p,(uint8_t)a,registers_[p][a]);
    } else if(fading_&&fadePause_) {fading_=false;fadeStep_=0;}}
void MegaDriveEngine::stop(){finished_=true;paused_=true;}
void MegaDriveEngine::applyFade() {
    // ~0.42 s, stepped in 60 Hz-equivalent increments; prototype, not SMPS exact.
    const int addition=std::min(127,fadeStep_*5);
    for(int port=0;port<2;port++)for(int ch=0;ch<3;ch++) {
        const uint8_t alg=registers_[port][0xb0+ch]&7;
        // YM2612 hardware operator slots: S1,S3,S2,S4 at offsets 0,4,8,12.
        const bool carriers[8][4]={{0,0,0,1},{0,0,0,1},{0,0,0,1},{0,0,0,1},
                                   {0,1,0,1},{0,1,1,1},{0,1,1,1},{1,1,1,1}};
        for(int op=0;op<4;op++)if(carriers[alg][op]){
            const int addr=0x40+ch+op*4;
            const uint8_t v=(uint8_t)std::min(127,(registers_[port][addr]&127)+addition);
            // Temporary fade value: do not overwrite the original TL shadow.
            ymWrites_.push_back({(uint8_t)port,(uint8_t)addr,v});
        }
    }
    if(++fadeStep_>=26){fading_=false;if(fadePause_)paused_=true;else finished_=true;}
}
void MegaDriveEngine::command() {
    if(cursor_>=vgm_.size()){finished_=true;return;}
    const uint8_t c=vgm_[cursor_++];
    auto need=[&](size_t n){if(cursor_+n>vgm_.size()){finished_=true;return false;}return true;};
    if(c==0x50) {if(need(1)){const uint8_t v=vgm_[cursor_++];if(!fading_)psg_.write(v);}}
    else if(c==0x52||c==0x53) {if(need(2)){uint8_t a=vgm_[cursor_++],v=vgm_[cursor_++];
        if(!(fading_&&(a==0x2a||a==0x2b)))writeYM(c==0x53,a,v);
    }}
    else if(c==0x61) {if(need(2)){wait_=u16(cursor_);cursor_+=2;}}
    else if(c==0x62)wait_=735;
    else if(c==0x63)wait_=882;
    else if(c>=0x70&&c<=0x7f)wait_=(c&15)+1;
    else if(c>=0x80&&c<=0x8f) {
        const uint8_t value=pcmIndex_<pcm_.size()?pcm_[pcmIndex_++]:0x80;
        if(!fading_)writeYM(0,0x2a,value);
        wait_=c&15;
    }
    else if(c==0x66) {if(loop_)cursor_=loop_;else finished_=true;}
    else if(c==0x67) {if(need(6)) {
        if(vgm_[cursor_++]!=0x66){finished_=true;return;}
        const uint8_t type=vgm_[cursor_++];const uint32_t size=u32(cursor_);cursor_+=4;
        if(!need(size))return;
        if(type==0)pcm_.assign(vgm_.begin()+cursor_,vgm_.begin()+cursor_+size);
        cursor_+=size;
    }}
    else if(c==0xe0) {if(need(4)){pcmIndex_=u32(cursor_);cursor_+=4;}}
    else if(c==0x4f || (c>=0x30&&c<=0x3f)) {if(need(1))cursor_++;}
    else if((c>=0x51&&c<=0x5f)||(c>=0xa0&&c<=0xbf)) {if(need(2))cursor_+=2;}
    else if(c>=0xc0&&c<=0xdf) {if(need(3))cursor_+=3;}
    else { // Do not reinterpret unknown commands as audio bytes.
        finished_=true;
    }
}
void MegaDriveEngine::tick(){
    if(paused_||finished_)return;
    int safety=0;
    while(!wait_&&!finished_&&++safety<20000)command();
    if(safety>=20000)finished_=true;
    if(wait_)--wait_;
    if(fading_ && ++fadeCounter_>=735) {fadeCounter_=0;applyFade();}
    ++samples_;
}
void MegaDriveEngine::sample(int16_t* output) {
    if(paused_||finished_){output[0]=output[1]=0;return;}
    clockFrac_+= (double)ymClock_/6.0/Rate;
    int clocks=(int)clockFrac_;clockFrac_-=clocks;
    int64_t left=0,right=0;Bit16s b[2]{};
    for(int i=0;i<clocks;i++){
        clockYMWrite();
        OPN2_Clock(&ym_,b);
        left+=b[0];right+=b[1];
    }
    const float fmL=clocks?(float)left/clocks/256.f:0;
    const float fmR=clocks?(float)right/clocks/256.f:0;
    const float psg=psg_.render();
    output[0]=(int16_t)clamp16((fmL*.77f+psg*.23f)*8500.f);
    output[1]=(int16_t)clamp16((fmR*.77f+psg*.23f)*8500.f);
}
void MegaDriveEngine::render(int16_t* dst,int frames){
    for(int i=0;i<frames;i++){tick();sample(dst+i*2);}
}
