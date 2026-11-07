#include "hardware/sonic_driver_image.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <vector>
int main(int argc,char** argv){
 if(argc!=2) { std::cerr<<"Usage: mda_test_real_sonic_driver user_rom.bin\n";return 2; }
 std::ifstream f(argv[1],std::ios::binary);
 if(!f) return 2;
 std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)),{});
 auto d=mda::findSonic3KZ80Bootstrap(rom);
 if(!d.found){std::cerr<<d.message<<"\n";return 1;}
 std::cout<<"Z80 program at 0x"<<std::hex<<d.romOffset<<" data at 0x"<<d.dataRomOffset
 <<" Z80 init jump target 0x"<<unsigned(d.initialRam[5]|d.initialRam[6]<<8)<<std::dec<<"\n";
 return 0;
}
