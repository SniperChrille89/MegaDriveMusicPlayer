# Mega Drive Audio – Windows desktop test (alpha)

This is a **native Windows** desktop application using Windows waveOut and the actual Nuked OPN2 YM2612 core bundled in `src/ym3438.c`. It is not an Android application, and it is not a ROM emulator.

## Build without installing anything locally

1. Create an empty GitHub repository and upload the contents of this folder, including the hidden `.github/workflows/windows.yml` file.
2. Open **Actions → Build Windows Player → Run workflow**.
3. After success, download the artifact **MegaDriveAudio-Windows-x64**. Extract it.
4. Run `MegaDriveAudio.exe`. Keep `unvgz.ps1` beside it for `.vgz` support.
5. Click **Open VGM / VGZ**, then **Play / Pause**, `<<`, `>>`, or double-click a track. Repeated Open adds tracks to the central playlist.

Alternatively, compile with Visual Studio Build Tools + CMake: `cmake -S . -B build -A x64 && cmake --build build --config Release`.

## Experimental behavior and limitations

- The player runs at 44.1 kHz stereo and sends VGM FM register writes to Nuked OPN2, YM2612 chip mode.
- PSG is an independently authored approximation and mix is not hardware calibrated.
- `.vgz` is decompressed using Windows PowerShell/.NET's built-in GZipStream; **PowerShell must be available**, and the included `unvgz.ps1` must remain beside the executable.
- VGM **PCM stream commands 0x90–0x95 are not supported**. Classic 0x80–0x8F DAC and type 00 data blocks are supported. Unsupported VGM commands may prematurely end playback.
- Track change uses experimental hardware-style FM TL fade (~0.42 s). Pause triggers the same approximate fade, Play resumes (not a verified Sonic pause emulation).
- Looping VGM loops indefinitely if a loop offset is present.
- No progress bar, region processing or hardware verification yet. PAL/NTSC follows the timing encoded in the VGM file; no fake PAL toggle.
- Audio latency depends on Windows waveOut buffers. Not yet tested on Windows here.
- Nuked core is LGPL 2.1+; see LICENSE file. Before distributing commercially, verify corresponding source/relinking obligations.

**This source package is intended as a buildable test, not as a production-ready player.**

## v0.2: YM2612 register-write timing fix (2026-11-04)

The v0.1 preview contained a real FM bug: sending the address and data to the
cycle-accurate Nuked OPN2 core without clocking the chip between them. That
caused FM register writes to be lost or misinterpreted; PSG could still play
while FM produced incorrect noise/impulses. v0.2 queues FM writes and services
them over chip clocks with separate address/data phases. This is a corrective
prototype, **not** a hardware-calibrated or Windows-tested release. The write
scheduler uses conservative 24-cycle spacing, which may defer bursts of register
writes slightly. Do not assume this timing is perfectly accurate yet.

The previous nine-second WAV is obsolete; use the newly exported `FM_fix_test.wav`
for an updated comparison, remembering that its mix and volume remain experimental.
