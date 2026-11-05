# Mega Drive Music Player v0.8 — separate console region and music timing

A Windows C++ VGM/VGZ player using Nuked OPN2. This version retains v0.7's 8x filtering, four volume sliders (PSG/FM/DAC/Master), and 4-second experimental hardware fade.

## New: independent controls above the track list

**Region:** PAL 50 Hz / NTSC 60 Hz. This changes the approximate chip oscillator ratios when the selected output region differs from the source VGM header rate.

**Music timing:**

- **Original VGM timing (default):** Does not modify stored VGM command waits, regardless of selected region. Safest and generally most faithful setting for soundtracks already recorded from game hardware.
- **Sonic 1 — PAL 50Hz VBlank slowdown:** For an NTSC (60 Hz) recording played as PAL, advances the VGM events at **5/6 speed**, creating ~20% longer tracks. PAL-sourced files are NOT slowed again when playing in PAL. Selecting NTSC with a 50 Hz source uses 6/5 speed.
- **Sonic 3 & Knuckles — PAL compensated (~97.2%):** For an NTSC recording played as PAL, advances events at **35/36 speed** (~97.22%). PAL-sourced tracks aren't slowed again; reverse conversion is applied for NTSC. This is a **driver-inspired approximation**, not a guaranteed bit-exact emulation of the exact original ROM/version's timing.

The 'source' status comes from the VGM header's 50/60 Hz hint at 0x24. It is not an authoritative claim about what the song's original game driver did. Files without a recognized rate are **assumed** to have 60 Hz source timing; inspect the source before applying a region conversion.

A VGM file contains timed hardware commands, not the original game's sound driver. No setting can reliably reverse-engineer arbitrary game-specific PAL optimizations. Use original VGM timing for unknown games and authentic PAL VGM recordings.

## Test and build

- `test_profiles.cpp` checks the duration of all 12 source-region/profile combinations using synthetic VGM files.
- `test_engine.cpp` is the existing engine smoke test; `test_fade.cpp` and `test_buses.cpp` test existing behavior.
- C++ engine/profile tests were compiled and run locally on Linux. **The Windows GUI has not been compiled here**, because this environment does not include a Windows cross compiler. The bundled `.github/workflows/windows.yml` builds the Windows EXE through GitHub Actions.

To update your repository, extract **the contents** of the ZIP to the root of your local `MegaDriveMusicPlayer` checkout, replacing existing files, then Commit and Push in GitHub Desktop. In GitHub Actions, use **Build Windows Player** and download the `MegaDriveAudio-Windows-x64` artifact. Keep `unvgz.ps1` alongside the EXE.

## Known limitations

- The PAL conversion is an approximation of game-driver timing, not a complete sound-driver emulator.
- Source-rate metadata may be absent or inaccurate.
- PSG, analog filtering, FM/DAC balance and certain VGM DAC streaming commands still require development and audio comparison to real hardware.
- 4-second hardware fade is experimental and pause does not yet duplicate the original Sonic Start button exactly.
- Nuked OPN2 is LGPL 2.1+; ensure library license and redistribution/relinking requirements are followed.
