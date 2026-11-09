# v0.37.1 — Windows build fix

Fixes GitHub Actions MSVC error C2065 in `src/hardware/rom_audio.cpp` line 33 (`fadeClock` undeclared identifier).

The static, offline `renderSonic3KRom()` renderer does not own a `OriginalFadeClock`; an accidental `fadeClock.prepare(bus)` call was inserted into that function. That single line has been removed. The streaming `Sonic3KStream::Impl::advanceFrames()` call to `fadeClock.prepare(bus)` remains intact.

No timing, fade algorithm, audio renderer, or GUI behavior changes.

Build verification: The supplied GitHub Actions error was examined. Full build could not run locally because the Z80 dependency is fetched from GitHub, which is not accessible in the working environment. GitHub Actions must confirm successful MSVC compilation.
