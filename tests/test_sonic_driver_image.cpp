#include "hardware/sonic_driver_image.h"
#include <vector>
#include <cassert>
int main(){
 std::vector<uint8_t> rom(100,0xff);
 assert(!mda::findSonic3KZ80Bootstrap(rom).found);
 // Literal-only 16-byte Kosinski test stream, followed by end marker.
 std::vector<uint8_t> compressed={0xff,0xff};
 for(int i=0;i<15;++i) compressed.push_back(uint8_t(i+1));
 compressed.push_back(0x02); compressed.push_back(0x00); // next descriptor prefetches before byte 16
 compressed.push_back(16);
 compressed.push_back(0);compressed.push_back(0);compressed.push_back(0); // match + terminator
 auto r=mda::decompressKosinski(compressed,0,8192);
 assert(r.ok && r.output.size()==16 && r.output[0]==1 && r.output[15]==16);
 auto trunc=mda::decompressKosinski(compressed,0,8);
 assert(!trunc.ok);
}
