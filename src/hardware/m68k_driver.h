#pragma once
#include "audio_bus.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mda {
// Musashi backend bridge. An experiment for running raw 68000 sound drivers.
// Not yet a Mega Drive ROM loader or complete 68000 address map.
class M68KDriver {
public:
    explicit M68KDriver(SoundBus& bus);
    void load(const uint8_t* data,size_t count);
    void reset();
    int runCycles(int cycles);
    uint32_t pc() const;
    uint8_t read8(uint32_t address) const;
    uint16_t read16(uint32_t address) const;
    void write8(uint32_t address,uint8_t value);
    void write16(uint32_t address,uint16_t value);
private:
    SoundBus& bus_;
    std::vector<uint8_t> rom_;
};
}
