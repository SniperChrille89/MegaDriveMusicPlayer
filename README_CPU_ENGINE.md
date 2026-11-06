# v0.14 – Real CPU audio backend, first working integration step

The graphical Windows VGM player remains preserved. Its internal playback engine is **still VGM-driven**. This revision adds an actual CPU-driven path as a separate, testable executable that executes original, newly-written Z80 machine instructions, emits chip register writes through the Mega Drive sound bus and serializes the cycle-timed writes to VGM. The resulting file can be opened in the existing player.

## What is now implemented

- SoundBus supports the Z80's Sega PSG port at `$7F11` in addition to YM2612 address/data ports `$4000–$4003`.
- A portable conversion from chronological CPU-cycle traces into a valid VGM file. Converts time using **absolute** CPU cycles to avoid accumulating floating-point rounding drift between events; retains ordered YM address/data and PSG writes.
- `mda_z80_generate_vgm` executes a small original Z80 instruction program with a real external Z80 emulator, and creates `Z80_CPU_Demo.vgm`. This is a PSG smoke test, **not Sonic music**. It demonstrates that the CPU is executing the commands, instead of replaying previously captured VGM instructions.
- `CPU Audio Engine (Z80)` GitHub Actions workflow compiles the Z80 backend, runs the tests and provides a `Real-Z80-Audio-Demo` artifact with a small sample VGM and generator EXE.
- Musashi Motorola 68000 adapter is **still experimental** and optional; it has NOT been verified in this revision.

## Windows GitHub Actions

There are **two workflows with different outputs**:

1. `Build Windows Player` produces `MegaDriveAudio-Windows-x64` including the regular playable `MegaDriveAudio.exe`.
2. `CPU Audio Engine (Z80)` produces `Real-Z80-Audio-Demo` including `mda_z80_generate_vgm.exe` and `Z80_CPU_Demo.vgm`, if the CPU build and tests succeed. Opening the VGM in MegaDriveAudio should reproduce the notes emitted by the CPU routine.

The CPU tests fetch the MIT-licensed `superzazu/z80` source during GitHub Actions. Internet access is required. No third-party CPU source is copied into this ZIP. Verify their current upstream license and preserve attributions when distributing CPU-enabled binaries.

## Known limitations – important

- Original Sonic 3K SMPS sequence playback is **not yet implemented**. It requires game-specific driver initialization, RAM/banking, Z80 IRQ, YM status/busy handling, DAC streaming and precise shared clock scheduling.
- Original Sonic 1 music requires a compatible 68000 sound driver and the 68000 CPU's memory/interrupt model, which is not implemented here yet.
- The experimental trace→VGM bridge intentionally does not solve all sound accuracy issues, and is **not** the final direct chip audio renderer.
- No new filter or compression trick is introduced; current FM/DAC distortion is not claimed fixed.
- The Z80 core and Musashi 68000 code are fetched only by optional CMake flags. The CPU test workflow is **unverified on Windows** in this environment because the SDK/compiler and GitHub network dependency are not available locally. Portable sound-bus and VGM-timing tests have been compiled and passed here.

## Local portable tests (no external dependencies)

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## CPU test (fetches external core)

```sh
cmake -S . -B build-z80 -DMDA_ENABLE_Z80=ON -DBUILD_TESTING=ON
cmake --build build-z80
ctest --test-dir build-z80 --output-on-failure
```
