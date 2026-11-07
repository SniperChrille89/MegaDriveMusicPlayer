#include "hardware/sonic_driver_image.h"
#include <vector>
#include <cassert>
int main(){
 std::vector<uint8_t> rom(0x10000,0xff);
 auto a=mda::findSonic3KZ80Bootstrap(rom);assert(!a.found);
 const size_t p=0x4000;
 const uint8_t head[]={0xf3,0xf3,0xed,0x56,0xc3,0x89,0x00};
 for(size_t i=0;i<sizeof(head);++i)rom[p+i]=head[i];
 auto b=mda::findSonic3KZ80Bootstrap(rom);
 assert(b.found && b.romOffset==p && b.initialRam.size()==0x1bf0);
 assert(b.initialRam[0]==0xf3 && b.initialRam[5]==0x89);
 rom[p+5]=0xff;rom[p+6]=0x1f;
 assert(!mda::findSonic3KZ80Bootstrap(rom).found);
}
