#include "src/engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int main(int argc,char** argv){
 if(argc!=2)return 2;
 std::ifstream f(argv[1],std::ios::binary);
 std::vector<uint8_t> data(std::istreambuf_iterator<char>{f},{});
 MegaDriveEngine player;std::string error;
 assert(player.load(data,error));
 int16_t out[2];
 for(int i=0;i<44100;i++)player.render(out,1);
 player.requestFade();assert(player.isFading());
 for(int i=0;i<176399;i++)player.render(out,1);
 assert(!player.isFinished());
 player.render(out,1);assert(player.isFinished());assert(!player.isFading());
 // Play after Stop should re-load the selected track, not resume a dead sequencer.
 assert(player.load(data,error)); assert(!player.isFinished());
 player.render(out,1);assert(player.position()==1);
 player.requestPause();assert(player.isPaused());assert(!player.isFading());
 player.resume();assert(!player.isPaused());
 std::cout<<"PASS: 40 fade steps / 4.0 s, replay after stop, pause unchanged\n";
}
