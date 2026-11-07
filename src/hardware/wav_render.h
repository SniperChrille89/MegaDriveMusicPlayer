#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mda {
struct WavStatistics { uint64_t frames=0; int peak=0; double rms=0.0; };
bool renderVgmToWav(const std::vector<uint8_t>& vgm, const std::string& path, std::string& error, WavStatistics* stats=nullptr);
}
