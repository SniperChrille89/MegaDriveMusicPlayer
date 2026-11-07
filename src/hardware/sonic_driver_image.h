#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mda {
struct SonicDriverImage {
  bool found = false;
  uint32_t romOffset = 0;
  std::vector<uint8_t> initialRam;
  std::string message;
};
// Diagnostic extraction ONLY. A ROM may hold compressed, relocated, or
// patched variants. Identifying a bootstrap is not proof of playable music.
SonicDriverImage findSonic3KZ80Bootstrap(const std::vector<uint8_t>& rom);
}
