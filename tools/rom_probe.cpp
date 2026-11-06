#include "hardware/audio_bus.h"
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

static std::string field(const std::vector<uint8_t>& bytes,size_t pos,size_t length) {
    std::string s;
    for(size_t i=pos;i<pos+length && i<bytes.size();++i) {
        unsigned char c=bytes[i];
        s.push_back(std::isprint(c) ? char(c) : ' ');
    }
    auto first=s.find_first_not_of(' ');
    if(first==std::string::npos)return "";
    auto last=s.find_last_not_of(' ');
    return s.substr(first,last-first+1);
}
int main(int argc,char** argv) {
    if(argc!=2) {std::cerr<<"Usage: mda_rom_probe <your_Sonic_ROM.bin>\n";return 2;}
    std::ifstream f(argv[1],std::ios::binary);
    if(!f) {std::cerr<<"Cannot open ROM file.\n";return 3;}
    std::vector<uint8_t> rom(std::istreambuf_iterator<char>{f},std::istreambuf_iterator<char>{});
    if(rom.size()<0x200 || rom.size()>32*1024*1024) {std::cerr<<"Invalid ROM size.\n";return 4;}
    const auto system=field(rom,0x100,16);
    if(system.find("SEGA") == std::string::npos) {std::cerr<<"No SEGA cartridge header; check byte ordering / file format.\n";return 5;}
    mda::SoundBus bus;
    bus.loadRom(rom.data(),rom.size());
    std::cout<<"ROM: "<<argv[1]<<"\n";
    std::cout<<"Size: "<<rom.size()<<" bytes\n";
    std::cout<<"System: "<<system<<"\n";
    std::cout<<"Domestic title: "<<field(rom,0x120,48)<<"\n";
    std::cout<<"Overseas title: "<<field(rom,0x150,48)<<"\n";
    std::cout<<"Product: "<<field(rom,0x180,14)<<"\n";
    std::cout<<"Region: "<<field(rom,0x1f0,16)<<"\n";
    uint32_t reset=(uint32_t(bus.romRead(4))<<24)|(uint32_t(bus.romRead(5))<<16)|(uint32_t(bus.romRead(6))<<8)|bus.romRead(7);
    std::cout<<"68000 reset vector: 0x"<<std::hex<<std::uppercase<<reset<<std::dec<<"\n";
    std::cout<<"ROM bus: "<<(bus.romRead(0x100)=='S' && bus.romRead(0x101)=='E' ? "OK":"FAIL")<<"\n";
    std::cout<<"ROM identified only. Driver extraction, boot initialization, Z80 RAM transfer, sound commands, and original SMPS playback remain TODO.\n";
    return 0;
}
