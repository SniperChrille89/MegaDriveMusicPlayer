#include "hardware/fade_transition.h"
#include <cassert>
int main(){
    using mda::shouldQueueNextSong;
    // No premature command or zero song command; normal Stop has no nextSong.
    assert(!shouldQueueNextSong(false,true,1,0x19));
    assert(!shouldQueueNextSong(true,false,1,0x19));
    assert(!shouldQueueNextSong(true,true,5,0x19));
    assert(!shouldQueueNextSong(true,true,1,0));
    // Dispatch before fade reaches zero, avoiding engine shutdown/reset.
    assert(shouldQueueNextSong(true,true,2,0x19));
    assert(shouldQueueNextSong(true,true,1,0x19));
    assert(shouldQueueNextSong(true,true,0,0x19));
}
