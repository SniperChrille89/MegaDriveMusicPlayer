#include "hardware/sonic_driver_image.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc,char**argv){
 if(argc<2){std::cerr<<"Usage: mda_s3k_rom_scan game.bin\n";return 2;}
 std::ifstream f(argv[1],std::ios::binary);
 if(!f){std::cerr<<"Cannot open ROM\n";return 2;}
 std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
 auto r=mda::findSonic3KZ80Bootstrap(bytes);
 if(!r.found){std::cerr<<r.message<<"\n";return 1;}
 std::cout<<"Found Sonic-style Z80 startup at ROM offset 0x"<<std::hex<<r.romOffset
 <<"; Z80 initial RAM image: "<<std::dec<<r.initialRam.size()<<" bytes\n";
 return 0;
}
