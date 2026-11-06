#include "engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>

static constexpr float levels[16] = {1,.794328f,.630957f,.501187f,.398107f,.316228f,.251189f,.199526f,.158489f,.125893f,.1f,.079433f,.063096f,.050119f,.039811f,0};
static constexpr int kFadeSteps = 40; // Sonic 3K: $28 (40) fade steps
static constexpr int kSamplesPerFadeStep = 4410; // six 60 Hz ticks = 0.1 s at 44.1 kHz
static constexpr float kOutputGain = 42500.f; // original v0.4 level, sliders control each path
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
    initialYMPrimeClocks_=0; initialYMPrimeWrites_=0; initialYMPrimed_=false;
    std::memset(registers_,0,sizeof registers_);
    clockFrac_=0; lowpassL_=lowpassR_=0; reconstructHead_=0; reconstruct_={}; eventFrac_=0; samples_=0; wait_=0; pcm_.clear();pcmIndex_=0;
    fading_=paused_=false;pauseTailFrames_=0;fadeStep_=fadeCounter_=0;finished_=false;
}
bool MegaDriveEngine::load(const std::vector<uint8_t>& bytes,std::string& error) {
    finished_=true; vgm_=bytes;
    if(vgm_.size()<0x40 || std::memcmp(vgm_.data(),"Vgm ",4)!=0) {error="Not a valid uncompressed VGM file";return false;}
    const uint32_t version=u32(8), data=u32(0x34);
    cursor_= version>=0x150 ? (data?0x34ull+data:0x40ull):0x40ull;
    if(cursor_>=vgm_.size()) {error="Invalid VGM data offset";return false;}
    ymClock_=u32(0x2c)&0x3fffffff;psgClock_=u32(0x0c)&0x3fffffff;
    if(!ymClock_)ymClock_=7670454;if(!psgClock_)psgClock_=3579545;
    // VGM header 0x24: recording frame rate for tempo scaling.
    // 0 means not specified: use 60Hz as a user-visible fallback, not as a fact.
    const uint32_t recordedRate=u32(0x24);
    sourceRateDeclared_=(recordedRate==50 || recordedRate==60);
    sourceRate_=sourceRateDeclared_ ? (int)recordedRate : 60;
    const uint32_t loopOffset=u32(0x1c);
    loop_=loopOffset?0x1cull+loopOffset:0;
    if(loop_>=vgm_.size())loop_=0;
    reset();return true;
}
void MegaDriveEngine::setRegion(int hz) {
    if (hz!=50 && hz!=60) return;
    targetRate_=hz;
    // Hardware clock adjustment is kept separate from music-command timing.
    // A file's 50/60 header is a hint; use original-timing mode if uncertain.
}
void MegaDriveEngine::setTimingProfile(int profile) {
    if (profile>=0 && profile<=2) timingProfile_=profile;
}
double MegaDriveEngine::eventSpeed() const {
    // The VGM header rate is a *hint* about the recording, not a
    // guaranteed measurement of the actual source soundtrack's tempo.
    // Original VGM mode is the safest general-purpose default.
    if (timingProfile_==0 || targetRate_==sourceRate_) return 1.0;
    // Sonic 1: 50 ticks / second where 60 were expected.
    // Sonic 3K PAL: reported extra driver tick cycle yields ~58.33/60.
    const double palRatio = timingProfile_==1 ? 5.0/6.0 : 35.0/36.0;
    return targetRate_==50 ? palRatio : 1.0/palRatio;
}
void MegaDriveEngine::writeYM(int port,uint8_t address,uint8_t value) {
    registers_[port][address]=value;
    // During hardware fade, keep the unfaded TL in the shadow registers.
    // Subsequent fade ticks will write the attenuated carrier TL values.
    // Never let an incoming VGM TL write momentarily restore full loudness.
    if (fading_ && address >= 0x40 && address <= 0x4f) return;
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
// VGM's time-zero register burst is conceptually an initial chip state, not
// hundreds of musical events that should consume the first few milliseconds.
// Big Arms has 326 YM writes at VGM sample 0. Serializing those writes into
// the normal realtime output produces a many-millisecond backlog, compressing
// the first FM note attacks together. Preclock them once, *without* advancing
// the VGM timeline or emitting PCM. Subsequent timed writes stay queued.
// This is a compatibility workaround for VGM's zero-time initialization,
// not a full replacement for a cycle-scheduled real-hardware bus.
void MegaDriveEngine::primeInitialYMRegisters() {
    if(initialYMPrimed_)return;
    initialYMPrimed_=true;
    initialYMPrimeWrites_=(int)ymWrites_.size();
    // Cap protects a malformed file from an unbounded startup delay.
    constexpr uint64_t MaxPrimeClocks=100000;
    Bit16s discarded[2]{};
    while((!ymWrites_.empty() || ymWriteStage_ || ymWriteDelay_) &&
          initialYMPrimeClocks_<MaxPrimeClocks) {
        clockYMWrite();
        OPN2_Clock(&ym_,discarded);
        ++initialYMPrimeClocks_;
    }
}
void MegaDriveEngine::startFade(bool /*pause*/) {
    if(finished_ || fading_)return;
    fading_=true;fadeStep_=0;fadeCounter_=0;
    // Kill new DAC playback and PSG immediately; let FM decay through TL changes.
    writeYM(0,0x2b,0);
    for(int ch=0;ch<4;ch++)psg_.write((uint8_t)(0x90+(ch<<5)+15));
}
void MegaDriveEngine::requestFade(){startFade(false);}
void MegaDriveEngine::requestPause(){
    if (finished_ || paused_ || fading_) return;
    // Freeze VGM time immediately. We do NOT start the 4-second song fade.
    // Natural YM envelopes must still be clocked for ~0.5 s after key-off;
    // this preserves Nuked's YM2612 output including ladder characteristics.
    paused_=true;
    pauseTailFrames_=PauseTailFrames;
    // Key-off all six FM channels (YM2612 channel codes: 0,1,2,4,5,6).
    // These are delivered through the normal cycle-accurate write queue.
    static constexpr uint8_t channels[6]={0,1,2,4,5,6};
    for(uint8_t ch:channels)ymWrites_.push_back({0,0x28,ch});
    // Stop PCM/DAC for the tail without changing the original VGM state.
    // The original DAC enable register is restored on resume.
    ymWrites_.push_back({0,0x2b,0});
    // PSG has no envelope release: make its outputs silent immediately.
    // A subsequent PSG volume command from the VGM can activate it again.
    for(int ch=0;ch<4;ch++)psg_.vol[ch]=15;
}
void MegaDriveEngine::resume(){
    if (!paused_) return;
    paused_=false;
    pauseTailFrames_=0;
    // Reinstate the VGM's DAC configuration, not stale DAC samples.
    ymWrites_.push_back({0,0x2b,registers_[0][0x2b]});
    // Deliberately DO NOT key-on the interrupted FM notes or restore PSG
    // volume: only subsequent VGM register events may trigger new notes.
}
void MegaDriveEngine::stop(){finished_=true;paused_=true;pauseTailFrames_=0;}
void MegaDriveEngine::setVolumes(int psg, int fm, int dac, int master) {
    psgGain_=std::clamp(psg,0,200)/100.f;
    fmGain_=std::clamp(fm,0,200)/100.f;
    dacGain_=std::clamp(dac,0,200)/100.f;
    masterGain_=std::clamp(master,0,200)/100.f;
}
void MegaDriveEngine::setAnalogLowpass(bool enabled, int strengthPercent) {
    analogLowpassEnabled_=enabled;
    lowpassStrength_=std::clamp(strengthPercent,5,95);
}
void MegaDriveEngine::applyFade() {
    // Sonic 3K zFadeOutMusic initializes $28 steps, 6 music-driver ticks
    // between each. zDoMusicFadeOut increases each FM track volume by 1
    // before sending new carrier TL values. VGM lacks the Z80 track RAM,
    // so this applies that +1 progression to the shadowed carrier TLs.
    const int addition=fadeStep_+1;
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
    if(++fadeStep_>=kFadeSteps){fading_=false;finished_=true;}
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
void MegaDriveEngine::tickVgm(){
    if(paused_||finished_)return;
    int safety=0;
    while(!wait_&&!finished_&&++safety<20000)command();
    if(safety>=20000)finished_=true;
    if(wait_)--wait_;
}
void MegaDriveEngine::tick(){
    if(paused_||finished_)return;
    // VGM waits are 44.1 kHz source-time units. Only emulate changes in
    // game-driver scheduling when explicitly requested: a 60 Hz VGM is NOT
    // automatically Sonic 1-style PAL slowed-down music.
    // Do not resample/pitch-shift the already mixed waveform.
    eventFrac_ += eventSpeed();
    while(eventFrac_ >= 1.0 && !finished_) {
        eventFrac_ -= 1.0;
        tickVgm();
    }
    // Before the first PCM sample, initialize the YM state from all VGM
    // writes at timestamp zero. Later writes keep their genuine VGM waits.
    if(samples_ == 0) primeInitialYMRegisters();
    // Fade duration is always 4 real seconds independent of region.
    if(fading_ && ++fadeCounter_>=kSamplesPerFadeStep) {fadeCounter_=0;applyFade();}
    ++samples_;
}
// Reconstruct the multiplexed YM2612 output before downsampling.  The
// previous engine averaged ~29 clock ticks directly into each 44.1kHz
// sample, a boxcar filter with very poor stopband rejection.  YM2612 DAC
// audio and some FM timbres therefore produced audible aliases.
static const std::array<float, 161>& reconstructionKernel() {
    static const auto taps=[] {
        std::array<float, 161> f{};
        // 19 kHz cutoff at 8*44.1kHz, Blackman window (high stopband rejection).
        constexpr double pi=3.14159265358979323846;
        constexpr double fc=19000.0/(44100.0*8.0);
        constexpr int N=161;
        double sum=0;
        for(int n=0;n<N;n++) {
            const double x=n-(N-1)*0.5;
            const double h=(x==0.0)?2.0*fc:std::sin(2.0*pi*fc*x)/(pi*x);
            const double w=0.42-0.5*std::cos(2.0*pi*n/(N-1))
                             +0.08*std::cos(4.0*pi*n/(N-1));
            f[n]=(float)(h*w); sum+=f[n];
        }
        for(float& a:f)a=(float)(a/sum);
        return f;
    }();
    return taps;
}
void MegaDriveEngine::sample(int16_t* output) {
    if(finished_ || (paused_ && pauseTailFrames_<=0)){output[0]=output[1]=0;return;}
    static constexpr double ntscYm=7670454.0, palYm=7600489.0;
    const double ymScale=(targetRate_==sourceRate_)?1.0:
        (targetRate_==50?palYm/ntscYm:ntscYm/palYm);
    static constexpr double ntscPsg=3579545.0, palPsg=3546895.0;
    const double psgScale=(targetRate_==sourceRate_)?1.0:
        (targetRate_==50?palPsg/ntscPsg:ntscPsg/palPsg);

    // Filter FM and DAC *separately* so their sliders remain independent.
    // Each internal clock contributes to exactly one bus based on the
    // Nuked output multiplexing slot (not the note's channel number).
    for(int sub=0;sub<Oversample;sub++) {
        clockFrac_+=((double)ymClock_*ymScale)/6.0/(Rate*Oversample);
        const int clocks=(int)clockFrac_;clockFrac_-=clocks;
        int64_t fmL=0,fmR=0,dacL=0,dacR=0;
        Bit16s b[2]{};
        for(int i=0;i<clocks;i++){
            clockYMWrite();
            const bool dacSlot=ym_.dacen && ((ym_.cycles>>2)==1);
            OPN2_Clock(&ym_,b);
            if(dacSlot){dacL+=b[0];dacR+=b[1];}
            else {fmL+=b[0];fmR+=b[1];}
        }
        // The YM2612 output is a switched time-division signal. Missing
        // DAC slots must contribute zero to DAC, not a held full DAC level.
        const float inv=clocks?1.0f/(clocks*256.0f):0.0f;
        reconstruct_[0][reconstructHead_]=(float)fmL*inv;
        reconstruct_[1][reconstructHead_]=(float)fmR*inv;
        reconstruct_[2][reconstructHead_]=(float)dacL*inv;
        reconstruct_[3][reconstructHead_]=(float)dacR*inv;
        reconstructHead_=(reconstructHead_+1)%FirTaps;
    }
    const auto& taps=reconstructionKernel();
    float bus[4]{};
    int index=(reconstructHead_+FirTaps-1)%FirTaps;
    for(int i=0;i<FirTaps;i++){
        const float a=taps[i];
        for(int j=0;j<4;j++)bus[j]+=reconstruct_[j][index]*a;
        if(--index<0)index=FirTaps-1;
    }
    psg_.clock=(double)psgClock_*psgScale;
    const float psg=(paused_?0.0f:psg_.render()*psgGain_*.23f);
    // Both buses originate in the same Nuked YM2612 output. Combined mode
    // uses a single gain for the full output and is the reference for checking
    // whether our experimental DAC/FM split colors the sound.
    const float left=(yamahaCombined_ ? (bus[0]+bus[2])*fmGain_
                      : bus[0]*fmGain_+bus[2]*dacGain_)*.77f;
    const float right=(yamahaCombined_ ? (bus[1]+bus[3])*fmGain_
                       : bus[1]*fmGain_+bus[3]*dacGain_)*.77f;
    float l=(left+psg)*kOutputGain*masterGain_;
    float r=(right+psg)*kOutputGain*masterGain_;
    // Optional output coloration, not the mandatory anti-alias filter above.
    // One-pole RC low-pass with a bounded cutoff, sample-rate independent.
    if (analogLowpassEnabled_) {
        const float cutoff=18000.f - (lowpassStrength_/100.f)*15000.f;
        const float alpha=1.f-std::exp(-2.f*3.14159265358979323846f*cutoff/Rate);
        lowpassL_ += alpha*(l-lowpassL_);
        lowpassR_ += alpha*(r-lowpassR_);
        l=lowpassL_;r=lowpassR_;
    } else { lowpassL_=l; lowpassR_=r; }
    output[0]=(int16_t)clamp16(l);
    output[1]=(int16_t)clamp16(r);
    if(paused_ && pauseTailFrames_>0) --pauseTailFrames_;
}
void MegaDriveEngine::render(int16_t* dst,int frames){
    for(int i=0;i<frames;i++){tick();sample(dst+i*2);}
}
