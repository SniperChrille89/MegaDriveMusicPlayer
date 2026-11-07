#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mda {
struct KosinskiResult {
 bool ok=false;
 std::vector<uint8_t> output;
 size_t bytesConsumed=0;
 std::string error;
};
KosinskiResult decompressKosinski(const std::vector<uint8_t>& rom, size_t start, size_t maxOutput);
struct SonicDriverImage {
 bool found=false;
 uint32_t romOffset=0; // compressed program stream
 uint32_t dataRomOffset=0; // compressed song/voice tables
 std::vector<uint8_t> initialRam; // final 8 KiB image, correctly mapped
 std::string message;
};
SonicDriverImage findSonic3KZ80Bootstrap(const std::vector<uint8_t>& rom);
}
