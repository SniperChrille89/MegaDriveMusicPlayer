# Mega Drive Music Player – Audio Hardware Engine (experimental v0.11)

The **existing Windows VGM player is unchanged**. Its familiar UI, VGM/VGZ reader, audio code, profiles, 4 sliders, and 4-second track-change fade are retained.

## New in this package

- `src/hardware/audio_bus.*`: portable, independently testable *sound-bus foundation*, RAM mirroring, Z80 YM2612 port writes, 68000-side PSG writes, and timestamped write trace.
- `src/hardware/z80_driver.*`: **real Z80 instruction execution** adapter using `superzazu/z80`, fetched optionally by CMake. Contains an independent CPU-to-YM-register smoke test.
- `src/hardware/m68k_driver.*`: experimental **Musashi 68000** integration for running instructions with callback-based 68000 memory access. Not yet complete or locally verified.
- `tests/test_bus.cpp` and `tests/test_z80_driver.cpp`.

## Important: not yet a Sonic 3K driver player

This version **does not** play SMPS/Sonic ROM music through the Z80/68000 yet. CPU adapters are separate from the Windows VGM player. These features still need implementation before driver playback works: Sonic RAM layout and driver initialization, 68000→Z80 sound commands and bus arbitration, cartridge memory banking, YM status/busy reads, Z80 interrupts at correct timing, a shared scheduler, real VBlank timing and DAC stream handling, and 68000 RAM and interrupts. Furthermore a VGM file does not contain the original driver code or per-channel SMPS music sequences.

The 68000 adapter is **early scaffolding**, not a ready full console emulator. The new bus cannot yet run actual Sonic sound drivers; the trace timestamp is coarse CPU-instruction timing, not a final master-clock hardware timestamp.

## Testing

Portable bus test (does not need internet):

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Optional CPU test (needs internet access at configure time):

```sh
cmake -S . -B build-z80 -DMDA_ENABLE_Z80=ON -DBUILD_TESTING=ON
cmake --build build-z80 --parallel
ctest --test-dir build-z80 --output-on-failure
```

The second command fetches upstream `superzazu/z80`. The **68000 module** can be configured with `-DMDA_ENABLE_M68K=ON`; its generator/build is as yet **unverified**. These options are OFF by default so the Windows player continues building.

## Next engineering tasks

1. Get the Z80 adapter passing on Windows CI and a synthetic SMPS-like command loop writing YM registers.
2. Implement Z80 sound RAM, 68k ROM bank switching, CPU clock/VBlank/IRQ and YM busy/status semantics.
3. Load compatible Sega driver and music sequences from user-supplied, legally usable data.
4. Compare five Big Arms loops against the original game; keep the standard VGM path separate and ensure there is no loop drift caused by scheduler rounding.
5. Validate YM2612 DAC timing/analog filtering against a trusted emulator before making sound-fidelity claims.

## Dependencies and licenses

- `src/ym3438.c` / `src/ym3438.h` – Nuked OPN2, LGPL 2.1+ (see `NUKED-LGPL-2.1.txt`).
- Optional `superzazu/z80` – MIT license, upstream <https://github.com/superzazu/z80>.
- Optional Musashi – permissive MIT-style license, upstream <https://github.com/kstenerud/Musashi>.

CPU dependency sources are **not included** in the archive: CMake fetches them if the respective option is enabled. Before distributing CPU-enabled binaries, retain the upstream license texts and notices. The existing player remains subject to its other component license obligations.
