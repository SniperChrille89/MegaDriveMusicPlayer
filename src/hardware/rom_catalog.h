#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mda {
struct RomTrack { uint8_t command; std::string label; };
struct RomCatalog {
    std::string domesticTitle;
    std::string overseasTitle;
    std::string productCode;
    std::string region;
    std::string platform;
    std::string profile;
    bool hasSoundTestProfile=false;
    bool valid=false;
    std::string error;
    std::vector<RomTrack> tracks;
};
// File must be an uncompressed binary Mega Drive cartridge image (.bin/.md).
// Parser intentionally does not execute untrusted ROM code.
RomCatalog inspectRom(const std::vector<uint8_t>& data);
}
