#pragma once
#include <cstdint>

namespace mda {
// A repeated selection of the queued track overrides an ongoing fade.
// After Stop there is no queued track, so the first new selection overrides.
constexpr bool shouldInterruptFade(bool fadeActive, int queuedIndex,
                                   int selectedIndex) noexcept {
    return fadeActive && selectedIndex >= 0 &&
           (queuedIndex < 0 || queuedIndex == selectedIndex);
}

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
