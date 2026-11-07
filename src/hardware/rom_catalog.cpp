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
        // Names correspond to the original Sonic 3 & Knuckles Sound Test ID
        // constants (sonicretro/skdisasm/sonic3k.constants.asm). IDs remain
        // visible for debugging; the labels do NOT change sound commands.
        static const char* const kTrackNames[] = {
            "Angel Island Zone - Act 1", // $01
            "Angel Island Zone - Act 2", // $02
            "Hydrocity Zone - Act 1", // $03
            "Hydrocity Zone - Act 2", // $04
            "Marble Garden Zone - Act 1", // $05
            "Marble Garden Zone - Act 2", // $06
            "Carnival Night Zone - Act 1", // $07
            "Carnival Night Zone - Act 2", // $08
            "Flying Battery Zone - Act 1", // $09
            "Flying Battery Zone - Act 2", // $0A
            "IceCap Zone - Act 1", // $0B
            "IceCap Zone - Act 2", // $0C
            "Launch Base Zone - Act 1", // $0D
            "Launch Base Zone - Act 2", // $0E
            "Mushroom Hill Zone - Act 1", // $0F
            "Mushroom Hill Zone - Act 2", // $10
            "Sandopolis Zone - Act 1", // $11
            "Sandopolis Zone - Act 2", // $12
            "Lava Reef Zone - Act 1", // $13
            "Lava Reef Zone - Act 2 / Hidden Palace", // $14
            "Sky Sanctuary Zone", // $15
            "Death Egg Zone - Act 1", // $16
            "Death Egg Zone - Act 2", // $17
            "Miniboss - Sonic & Knuckles", // $18
            "End Boss / Big Arms", // $19
            "The Doomsday Zone", // $1A
            "Pachinko Bonus Stage", // $1B
            "Special Stage", // $1C
            "Slot Machine Bonus Stage", // $1D
            "Gumball Bonus Stage", // $1E
            "Knuckles Theme", // $1F
            "Azure Lake Zone", // $20
            "Balloon Park Zone", // $21
            "Desert Palace Zone", // $22
            "Chrome Gadget Zone", // $23
            "Endless Mine Zone", // $24
            "Title Screen", // $25
            "Sonic 3 Credits", // $26
            "Game Over", // $27
            "Continue", // $28
            "Act Clear", // $29
            "Extra Life", // $2A
            "Chaos Emerald", // $2B
            "Invincibility", // $2C
            "Competition Menu", // $2D
            "Miniboss - Robotnik", // $2E
            "Data Select", // $2F
            "Final Boss", // $30
            "Drowning", // $31
            "Ending", // $32
        };
        static_assert(sizeof(kTrackNames)/sizeof(kTrackNames[0]) == 0x32,
                      "Sonic 3K Sound Test catalog must have $01 through $32");
        const char hex[]="0123456789ABCDEF";
        for(int i=1;i<=0x32;++i){
            std::string label=kTrackNames[i-1];
            label += " [$";
            label += hex[(i>>4)&15];
            label += hex[i&15];
            label += ']';
            r.tracks.push_back({uint8_t(i),label});
        }
        r.tracks.push_back({0xDC,"Sonic & Knuckles Credits [$DC]"});
    } else if(contains(combined,"SONIC THE HEDGEHOG") || contains(combined,"SONIC 1")){
        r.profile="Sonic 1 (68000 driver; not implemented)";
    } else {
        r.profile="Unknown sound driver (no song catalog)";
    }
    return r;
}
}
