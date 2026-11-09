#include "stem_export.h"
#include "rom_audio.h"
#include "../engine.h"
#include <array>
#include <algorithm>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace mda {
namespace {
void write16(std::ostream& out,uint16_t n){char b[2]={char(n),char(n>>8)};out.write(b,2);}
void write32(std::ostream& out,uint32_t n){char b[4]={char(n),char(n>>8),char(n>>16),char(n>>24)};out.write(b,4);}
void wavHeader(std::ostream& out,uint32_t frames){
    constexpr uint16_t channels=2, bits=16;
    constexpr uint32_t rate=44100, bytesPerFrame=channels*bits/8;
    const uint32_t dataSize=frames*bytesPerFrame;
    out.write("RIFF",4);write32(out,36+dataSize);out.write("WAVEfmt ",8);
    write32(out,16);write16(out,1);write16(out,channels);
    write32(out,rate);write32(out,rate*bytesPerFrame);
    write16(out,bytesPerFrame);write16(out,bits);
    out.write("data",4);write32(out,dataSize);
}
constexpr const char* names[7]={
 "01 - SN76489.wav", "02 - YM2612 FM1.wav", "03 - YM2612 FM2.wav",
 "04 - YM2612 FM3.wav", "05 - YM2612 FM4.wav", "06 - YM2612 FM5.wav",
 "07 - YM2612 DAC.wav"};
}
StemExportResult exportSonic3KStems(const StemExportSettings& opt,
                                    const std::function<void(unsigned)>& progress){
    StemExportResult result;
    if(opt.rom.empty()||opt.destination.empty() ||
       (opt.region!=50&&opt.region!=60) ||
       opt.durationSeconds==0||opt.durationSeconds>3600){
        result.error="Invalid ROM, destination, duration or region";return result;
    }
    const uint32_t total=opt.durationSeconds*44100u;
    std::array<std::ofstream,7> files;
    try{
        std::filesystem::create_directories(opt.destination);
        for(int i=0;i<7;i++){
            auto path=opt.destination/names[i];
            // Truncate prior export with the same title.
            files[i].open(path,std::ios::binary|std::ios::trunc);
            if(!files[i]) throw std::runtime_error("Failed to create WAV file");
            wavHeader(files[i],total);
        }
        Sonic3KStream session(opt.rom,opt.musicId,opt.region);
        MegaDriveEngine engine;
        engine.setVolumes(opt.gains[0],opt.gains[1],opt.gains[2],opt.gains[3]);
        engine.setRegion(opt.region);
        engine.setTimingProfile(0);
        bool first=true;
        constexpr unsigned block=512;
        std::array<int16_t,block*14> pcm{};
        std::array<std::array<int16_t,block*2>,7> stemPcm{};
        while(result.frames<total){
            // Build only the next 5 video frames (83/100 ms).
            // No reloading ROM, no YM chip reset, no repeated song runs.
            auto chunk=session.advanceFrames(5);
            if(!chunk.error.empty()) throw std::runtime_error(chunk.error);
            if(first){
                std::string err;
                if(!engine.load(chunk.vgm,err))throw std::runtime_error(err);
                engine.setVolumes(opt.gains[0],opt.gains[1],opt.gains[2],opt.gains[3]);
                engine.setRegion(opt.region);
                engine.setTimingProfile(0);
                engine.enableRomStream();
                first=false;
            }else if(!engine.appendRomCommands(chunk.vgm,chunk.totalSamples)){
                throw std::runtime_error("Failed to append original Z80 stream");
            }
            while(result.frames<total && engine.position()<chunk.totalSamples){
                const unsigned n=static_cast<unsigned>(std::min<uint64_t>(
                  {block,uint64_t(total-result.frames),chunk.totalSamples-engine.position()}));
                if(!n)break;
                engine.renderStems(pcm.data(),int(n));
                for(int k=0;k<7;k++){
                    for(unsigned f=0;f<n;f++){
                        stemPcm[k][f*2]=pcm[f*14+k*2];
                        stemPcm[k][f*2+1]=pcm[f*14+k*2+1];
                    }
                    // Windows x64 is little-endian; emit one interleaved stereo block.
                    files[k].write(reinterpret_cast<const char*>(stemPcm[k].data()),
                                   std::streamsize(n*4));
                    if(!files[k])throw std::runtime_error("WAV file write failed");
                }
                result.frames+=n;
            }
            if(progress)progress(unsigned(result.frames*100/total));
        }
        for(auto& f:files){f.flush();if(!f)throw std::runtime_error("WAV write error");}
        result.ok=true;
    }catch(const std::exception& e){result.error=e.what();}
    for(auto& f:files)if(f.is_open())f.close();
    if(!result.ok){
        // Avoid leaving corrupt, misleading 'complete' files.
        for(const auto* name:names){std::error_code ec;std::filesystem::remove(opt.destination/name,ec);}
    }
    return result;
}
}
