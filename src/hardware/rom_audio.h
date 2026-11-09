#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mda {
struct RomAudioResult {
    std::vector<uint8_t> vgm;
    std::string error;
    uint64_t ymWrites=0, psgWrites=0;
    uint64_t totalSamples=0;
};
// Render user-supplied Sonic 3K ROM by executing the original Z80 sound driver.
// Returns a memory-only VGM trace, no intermediate disk file. Non-realtime v0.22.
RomAudioResult renderSonic3KRom(const std::vector<uint8_t>& rom, uint8_t musicId,
                                  unsigned seconds=60, int region=50);
}

// A persistent Sonic 3K session: Z80 RAM, bank, CPU and driver state survive
// between blocks. No ROM or copyrighted game data is embedded in the program.
#include <memory>
namespace mda {
struct RomAudioChunk {
    std::vector<uint8_t> vgm;  // first chunk = VGM; later chunks = VGM commands + 0x66
    std::string error;
    uint64_t totalSamples=0;
    bool fadeFinished=false;
    bool songSwitched=false; // A queued music ID was dispatched in the live Z80 session.
    uint8_t switchedMusicId=0; // Disambiguates a completed earlier async chunk.
};
class Sonic3KStream {
public:
    explicit Sonic3KStream(const std::vector<uint8_t>& rom, uint8_t id, int region);
    ~Sonic3KStream();
    Sonic3KStream(const Sonic3KStream&) = delete;
    Sonic3KStream& operator=(const Sonic3KStream&) = delete;
    RomAudioChunk advance(unsigned seconds);
    // Advance by exact game video frames (PAL 20 ms / NTSC 16.67 ms).
    // Allows low-latency command dispatch without 1-second pre-generation.
    RomAudioChunk advanceFrames(unsigned frames);
    // Post an original Sonic 3K sound command, e.g. $E1 fade-out.
    void sendCommand(uint8_t command);
    // Execute original $E1 with FM steps paced over exactly this many seconds.
    void beginFade(unsigned seconds);
    // Switch in-place near the end of the original driver fade, without a chip reset.
    void queueSongAfterFade(uint8_t musicId);
    void cancelQueuedSong();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
