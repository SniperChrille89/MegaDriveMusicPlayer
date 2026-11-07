#include "hardware/rom_catalog.h"
#include <cassert>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
int main(int argc,char** argv){
 std::vector<uint8_t> rom(0x400,0);
 auto write=[&](size_t offset,std::string s){for(size_t i=0;i<s.size();++i)rom[offset+i]=s[i];};
 auto invalid=mda::inspectRom(rom); assert(!invalid.valid);
 write(0x100,"SEGA MEGA DRIVE");write(0x150,"SONIC 3 & KNUCKLES");write(0x1f0,"E");
 auto found=mda::inspectRom(rom);
 assert(found.valid && found.hasSoundTestProfile);
 assert(found.tracks.size()==51);
 assert(found.tracks[0].command==1 && found.tracks[49].command==0x32);
 assert(found.tracks[50].command==0xdc);
 if(argc>1){
   std::ifstream f(argv[1],std::ios::binary);
   if(!f){std::cerr<<"Cannot open ROM\n";return 2;}
   std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});
   auto c=mda::inspectRom(b);
   std::cout<<"ROM: "<<(c.valid?"valid":"invalid")<<"\nTitle: "<<c.overseasTitle
       <<"\nProfile: "<<c.profile<<"\nTracks: "<<c.tracks.size()<<"\n";
   if(!c.valid)return 3;
 }
 std::cout<<"ROM catalog parser: PASS\n";
}
