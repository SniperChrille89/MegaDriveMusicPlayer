#include "audio_bus.h"
#include <algorithm>
#include <utility>

namespace mda {
void SoundBus::reset() {
    ram_.fill(0); ymPorts_.fill(0);
    trace_.clear(); cycle_=0; bank_=0;
}
void SoundBus::setSink(WriteSink sink) { sink_=std::move(sink); }
void SoundBus::loadZ80(const uint8_t* source, size_t count, uint16_t start) {
    if (!source || start >= ram_.size()) return;
    std::copy_n(source, std::min(count, ram_.size()-start), ram_.begin()+start);
}
uint8_t SoundBus::z80Read(uint16_t a) const {
    if (a < 0x4000) return ram_[a & 0x1fff];
    if (a >= 0x4000 && a < 0x6000) return ymPorts_[a & 3];
    // Banked 68k memory at $8000-$FFFF needs ROM/memory-bus callback in next stage.
    return 0xff;
}
void SoundBus::emit(Chip chip, uint16_t port, uint8_t value) {
    SoundWrite w{chip,cycle_,port,value}; trace_.push_back(w);
    if (sink_) sink_(w);
}
void SoundBus::z80Write(uint16_t a, uint8_t v) {
    if (a < 0x4000) {ram_[a & 0x1fff]=v; return;}
    if (a >= 0x4000 && a < 0x6000) {ymPorts_[a & 3]=v; emit(Chip::YM2612,a & 3,v); return;}
    if (a >= 0x6000 && a < 0x6100) {bank_=uint16_t((bank_ >> 1) | ((v & 1) << 8)); return;}
    // The Mega Drive Z80 can also reach the PSG through the VDP's $7F11 port.
    if ((a & 0xff00) == 0x7f00 && (a & 0x1f) == 0x11) { emit(Chip::PSG,0,v); return; }
}
void SoundBus::m68kWrite8(uint32_t a, uint8_t v) {
    a &= 0xffffff;
    if (a >= 0xa00000 && a <= 0xa01fff) {z80Write(uint16_t(a),v); return;}
    if (a >= 0xa04000 && a <= 0xa04003) {z80Write(uint16_t(0x4000+(a&3)),v); return;}
    if (a >= 0xc00010 && a <= 0xc0001f) {emit(Chip::PSG,0,v); return;}
}
uint8_t SoundBus::m68kRead8(uint32_t a) const {
    a &= 0xffffff;
    if (a >= 0xa00000 && a <= 0xa01fff) return z80Read(uint16_t(a));
    if (a >= 0xa04000 && a <= 0xa04003) return z80Read(uint16_t(0x4000+(a&3)));
    return 0xff;
}
}
