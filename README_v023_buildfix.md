# v0.23 build fix (2026-11-07)

Fixes MSVC error C2672 and cascading C2737 in `src/player.cpp` line 133 when compiling the timeline:

```cpp
const int y=19,xa=16,xb=std::max(xa+1,static_cast<int>(rc.right)-16);
```

`RECT::right` is `LONG`, so converting it to `int` makes both arguments to `std::max` the same type. No changes to Z80, FM, PSG or music playback. UNICODE macro redefinition and HMENU pointer-size diagnostics in the log are warnings, not this build failure. Windows compilation still needs to be verified via GitHub Actions.
