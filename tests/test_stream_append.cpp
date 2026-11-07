#include "engine.h"
#include <cassert>
#include <cstring>
#include <vector>
#include <iostream>
static std::vector<uint8_t> header(){
 std::vector<uint8_t> v(0x40,0);
 std::memcpy(v.data(),"Vgm ",4);v[8]=0x50;v[9]=1;v[0x34]=0x0c;
 const unsigned psg=3579545,ym=7670454;
 for(int i=0;i<4;i++){v[0x0c+i]=uint8_t(psg>>(8*i));v[0x2c+i]=uint8_t(ym>>(8*i));}
 return v;
}
static void wait(std::vector<uint8_t>& v,unsigned samples){
 while(samples){unsigned n=std::min(samples,65535u);v.push_back(0x61);v.push_back(uint8_t(n));v.push_back(uint8_t(n>>8));samples-=n;}
}
static void fix(std::vector<uint8_t>& v,unsigned samples){
 unsigned eof=(unsigned)v.size()-4;
 for(int i=0;i<4;i++){v[4+i]=uint8_t(eof>>(8*i));v[0x18+i]=uint8_t(samples>>(8*i));}
}
int main(){
 auto a=header();a.insert(a.end(),{0x50,0x82,0x50,0x01,0x50,0x90});wait(a,44100);a.push_back(0x66);fix(a,44100);
 std::vector<uint8_t> b;wait(b,44100);b.push_back(0x66);
 auto whole=a;whole.pop_back();whole.insert(whole.end(),b.begin(),b.end());fix(whole,88200);
 MegaDriveEngine streamed,reference;
 std::string error;
 assert(streamed.load(a,error));streamed.enableRomStream();
 assert(reference.load(whole,error));
 std::vector<int16_t> x(24000*2),y(24000*2);
 streamed.render(x.data(),24000);reference.render(y.data(),24000);
 assert(x==y);
 assert(streamed.appendRomCommands(b,88200));
 streamed.render(x.data(),24000);reference.render(y.data(),24000);
 assert(x==y); // includes the old 1s terminator boundary, must be bit identical
 assert(!streamed.isFinished());
 std::cout<<"Streaming extension keeps 44.1 kHz chip output bit-identical across boundary\n";
}
