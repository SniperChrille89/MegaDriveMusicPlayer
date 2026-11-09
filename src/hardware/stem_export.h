#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace mda {
struct StemExportSettings {
    std::vector<uint8_t> rom;
    uint8_t musicId=1;
    int region=50;
    unsigned durationSeconds=180;
    std::filesystem::path destination;
    std::array<int,4> gains{{75,120,110,100}}; // PSG, FM, DAC, master
};
struct StemExportResult {bool ok=false;std::string error;uint64_t frames=0;};
// This session runs the real Z80 once, clocks a single YM2612 instance,
// and writes all seven synchronized PCM outputs in one rendering pass.
StemExportResult exportSonic3KStems(const StemExportSettings& settings,
                                   const std::function<void(unsigned)>& onProgress={});
}
