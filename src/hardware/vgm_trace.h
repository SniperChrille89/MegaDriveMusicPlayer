#pragma once
#include "audio_bus.h"
#include <cstdint>
#include <string>
#include <vector>

namespace mda {
// Converts actual CPU-emitted, cycle-stamped hardware writes into a standard
// VGM command stream. A diagnostic bridge, not an SMPS driver interpreter.
std::vector<uint8_t> traceToVgm(const std::vector<SoundWrite>& trace,
                                uint64_t totalCycles,
                                uint32_t cpuClock = 3579545,
                                uint32_t ymClock = 7670454,
                                uint32_t psgClock = 3579545);
bool writeVgm(const std::string& path, const std::vector<uint8_t>& bytes);
}
