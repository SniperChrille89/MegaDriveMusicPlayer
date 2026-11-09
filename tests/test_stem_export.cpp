#include "hardware/stem_export.h"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <filesystem>

int main(){
    mda::StemExportSettings opt;
    auto invalid=mda::exportSonic3KStems(opt);
    assert(!invalid.ok && !invalid.error.empty());
    // Optional genuine ROM integration smoke test. The proprietary game ROM
    // is supplied externally by the tester and NEVER enters this project.
    const char* romPath=std::getenv("MDA_TEST_SONIC3K_ROM");
    if(!romPath){
       std::cout<<"Settings rejection passed; ROM integration needs MDA_TEST_SONIC3K_ROM\n";
       return 0;
    }
    std::ifstream f(romPath,std::ios::binary);
    if(!f){std::cerr<<"ROM path cannot be read\n";return 2;}
    opt.rom.assign(std::istreambuf_iterator<char>(f),{});
    opt.musicId=0x01;
    opt.region=50;
    opt.durationSeconds=2;
    opt.destination=std::filesystem::temp_directory_path()/"mda_stems_integration_test";
    auto result=mda::exportSonic3KStems(opt);
    if(!result.ok){std::cerr<<result.error<<'\n';return 3;}
    if(result.frames!=88200)return 4;
    const char* filenames[7]={
      "01 - SN76489.wav","02 - YM2612 FM1.wav","03 - YM2612 FM2.wav",
      "04 - YM2612 FM3.wav","05 - YM2612 FM4.wav","06 - YM2612 FM5.wav",
      "07 - YM2612 DAC.wav"};
    for(const char* name:filenames){
       auto file=opt.destination/name;
       if(!std::filesystem::exists(file) ||
          std::filesystem::file_size(file)!=44u+88200u*4u){
           std::cerr<<"Invalid stem length: "<<file<<'\n';return 5;
       }
       std::filesystem::remove(file);
    }
    std::filesystem::remove(opt.destination);
    std::cout<<"All seven ROM-generated WAVs have identical 2-second length\n";
    return 0;
}
