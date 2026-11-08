#pragma once
#include <cstdint>

namespace mda {
// A game-track change can be delivered while the original driver's fade
// still has steps remaining. Never use this for a normal Stop command.
constexpr bool shouldQueueNextSong(bool fadeStarted,
                                   bool fadeCountObserved,
                                   uint8_t fadeStepsRemaining,
                                   uint8_t nextSongId) noexcept {
    return fadeStarted && fadeCountObserved && nextSongId != 0 &&
           fadeStepsRemaining <= 2;
}
}
