#include "rom_catalog.h"
#include <algorithm>
#include <cctype>
namespace mda {
static std::string header(const std::vector<uint8_t>& d,size_t pos,size_t count){
    if(pos+count>d.size())return {};
    std::string v; v.reserve(count);
    for(size_t i=0;i<count;++i){unsigned char c=d[pos+i];v.push_back(c>=32&&c<127?char(c):' ');}
    while(!v.empty()&&v.back()==' ')v.pop_back();
    auto p=v.find_first_not_of(' ');if(p==std::string::npos)return {};
    return v.substr(p);
}
static bool contains(std::string s,std::string sub){
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::toupper(c));});
    return s.find(sub)!=std::string::npos;
}
RomCatalog inspectRom(const std::vector<uint8_t>& data){
    RomCatalog r;
    if(data.size()<0x200 || data.size()>32u*1024u*1024u){r.error="Not a plausible Mega Drive ROM size";return r;}
    r.platform=header(data,0x100,16);
    if(!contains(r.platform,"SEGA")){r.error="SEGA cartridge header missing at 0x100 (SMD/interleaved ROMs are not supported)";return r;}
    r.valid=true;
    r.domesticTitle=header(data,0x120,48);
    r.overseasTitle=header(data,0x150,48);
    r.productCode=header(data,0x180,14);
    r.region=header(data,0x1f0,16);
    std::string combined=r.domesticTitle+" "+r.overseasTitle;
    if(contains(combined,"SONIC")&&(contains(combined,"KNUCKLES") || contains(combined,"SONIC 3") || contains(combined,"SONIC THE HEDGEHOG 3"))){
        r.profile="Sonic 3 / Sonic & Knuckles family (candidate)";
        r.hasSoundTestProfile=true;
        // Sound driver command IDs from Sonic 3 & Knuckles Z80 disassembly.
        // This is an ID browser, not yet an executable driver/song parser.
        for(int i=1;i<=0x32;++i){char id[16];
            const char hex[]="0123456789ABCDEF";
            id[0]='M';id[1]='u';id[2]='s';id[3]='i';id[4]='c';id[5]=' ';
            id[6]='0';id[7]='x';id[8]=hex[(i>>4)&15];id[9]=hex[i&15];id[10]=0;
            r.tracks.push_back({uint8_t(i),id});
        }
        r.tracks.push_back({0xDC,"Credits 0xDC (S&K driver)"});
    } else if(contains(combined,"SONIC THE HEDGEHOG") || contains(combined,"SONIC 1")){
        r.profile="Sonic 1 (68000 driver; not implemented)";
    } else {
        r.profile="Unknown sound driver (no song catalog)";
    }
    return r;
}
}
