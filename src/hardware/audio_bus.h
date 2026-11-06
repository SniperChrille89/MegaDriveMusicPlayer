#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

// Minimal Mega Drive sound address-space foundation. This is not a full console.
// All emitted writes are ordered and have the running CPU cycle as timestamp.
namespace mda {
enum class Chip : uint8_t { YM2612, PSG };
struct SoundWrite {
    Chip chip;
    uint64_t z80Cycle;
    uint16_t port; // YM: 0..3 (address/data for port A/B). PSG: 0
    uint8_t value;
};
class SoundBus {
public:
    using WriteSink = std::function<void(const SoundWrite&)>;
    void reset();
    void setSink(WriteSink sink);
    void setCycle(uint64_t cycle) { cycle_ = cycle; }
    uint64_t cycle() const { return cycle_; }
    void loadZ80(const uint8_t* source, size_t count, uint16_t start = 0);
    // Game cartridge data is read-only and supplied by the user; no ROM included.
    void loadRom(const uint8_t* source, size_t count);
    size_t romSize() const { return rom_.size(); }
    uint32_t z80BankBase() const { return uint32_t(bank_) << 15; }
    uint8_t romRead(uint32_t address) const;
    uint8_t z80RamRead(uint16_t addr) const { return ram_[addr & 0x1fff]; }
    uint8_t z80Read(uint16_t addr) const;
    void z80Write(uint16_t addr, uint8_t value);
    // 68000-side minimal sound ports. Not a full memory map.
    void m68kWrite8(uint32_t addr, uint8_t value);
    uint8_t m68kRead8(uint32_t addr) const;
    const std::vector<SoundWrite>& trace() const { return trace_; }
    void clearTrace() { trace_.clear(); }
    // Z80 bank register: bit stream through $6000 (Mega Drive behavior).
    uint16_t bank() const { return bank_; }
private:
    void emit(Chip chip, uint16_t port, uint8_t value);
    std::array<uint8_t,8192> ram_{};
    std::array<uint8_t,4> ymPorts_{};
    std::vector<uint8_t> rom_;
    std::vector<SoundWrite> trace_;
    WriteSink sink_;
    uint64_t cycle_=0;
    uint16_t bank_=0;
};
}
