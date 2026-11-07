#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mda {
struct RomAudioResult {
    std::vector<uint8_t> vgm;
    std::string error;
    uint64_t ymWrites=0, psgWrites=0;
};
// Render user-supplied Sonic 3K ROM by executing the original Z80 sound driver.
// Returns a memory-only VGM trace, no intermediate disk file. Non-realtime v0.22.
RomAudioResult renderSonic3KRom(const std::vector<uint8_t>& rom, uint8_t musicId,
                                  unsigned seconds=60, int region=50);
}
