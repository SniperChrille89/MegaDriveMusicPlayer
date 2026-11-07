#include "rom_audio.h"
#include "sonic_driver_image.h"
#include "z80_driver.h"
#include "vgm_trace.h"
#include <algorithm>
#include <exception>
namespace mda {
RomAudioResult renderSonic3KRom(const std::vector<uint8_t>& rom, uint8_t musicId,
                                unsigned seconds, int region) {
    RomAudioResult result;
    if (rom.empty() || seconds==0 || seconds>300 || (region!=50 && region!=60)) {
        result.error="Invalid ROM, duration, or region"; return result;
    }
    auto image=findSonic3KZ80Bootstrap(rom);
    if(!image.found) {result.error="No supported Sonic 3K sound driver found: "+image.message;return result;}
    try {
        SoundBus bus;
        Z80Driver cpu(bus);
        cpu.load(image.initialRam.data(),image.initialRam.size());
        bus.loadRom(rom.data(),rom.size());
        cpu.runCycles(500000);
        bus.z80Write(0x1c0a,musicId); // Sonic 3K zMusicNumber
        bus.z80Write(0x1c02,region==50?1:0); // original Z80 PAL flag
        // 3,546,895 Hz nominal PAL clock; NTSC clock approx. 3,579,545 Hz.
        const uint32_t cpuClock=region==50?3546895u:3579545u;
        const uint32_t ymClock=region==50?7600489u:7670454u;
        const unsigned frames=seconds*unsigned(region);
        for(unsigned frame=0;frame<frames;++frame) {
            cpu.requestInterrupt();
            // Carry fractional cycles across frames instead of discarding remainder.
            const uint64_t target=(uint64_t(frame+1)*cpuClock)/unsigned(region);
            if(cpu.cycles()<target+500000ull) cpu.runCycles(target+500000ull-cpu.cycles());
        }
        for(const auto& w: bus.trace())
            (w.chip==Chip::YM2612?result.ymWrites:result.psgWrites)++;
        if(result.ymWrites==0 && result.psgWrites==0) {
            result.error="Driver did not emit sound chip writes"; return result;
        }
        result.vgm=traceToVgm(bus.trace(),cpu.cycles(),cpuClock,ymClock,cpuClock);
    } catch(const std::exception& ex) {result.error=ex.what();}
    return result;
}
}
