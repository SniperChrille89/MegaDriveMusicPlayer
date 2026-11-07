#pragma once
#include "audio_bus.h"
#include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif
#include "z80.h"
#ifdef __cplusplus
}
#endif

namespace mda {
// Actual Z80 instruction execution, not an SMPS interpreter.
// Caller supplies a Z80 machine-code driver; CPU starts at $0000.
// This is a research/test integration, not enabled in the GUI yet.
class Z80Driver {
public:
    explicit Z80Driver(SoundBus& bus);
    void reset();
    void load(const uint8_t* data, size_t count);
    uint64_t runCycles(uint64_t count);
    uint16_t pc() const { return core_.pc; }
    uint64_t cycles() const { return core_.cyc; }
private:
    static uint8_t read(void* context,uint16_t address);
    static void write(void* context,uint16_t address,uint8_t value);
    static uint8_t in(z80*,uint8_t);
    static void out(z80*,uint8_t,uint8_t);
    SoundBus& bus_;
    z80 core_{};
};
}
