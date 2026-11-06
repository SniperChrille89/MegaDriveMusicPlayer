# Mega Drive Music Player v0.15 – Stop and Sonic 3K fade cadence

## Changes
- A dedicated **Stop** button next to previous / Play-Pause / next.
- **Stop**, **previous**, **next**, and choosing another track all request the same FM register fade. After Stop, playback remains stopped. Press **Play** to start the selected track from its beginning.
- Fade timing is based on Sonic 3 & Knuckles Z80 driver `zFadeOutMusic` and `zDoMusicFadeOut`: 0x28 (40) steps, with a six-tick delay at 60 Hz between volume changes (4.0 s total).
- Original Z80 routine mutes DAC and PSG at fade initiation. FM track volumes increase by one unit per step and finally `zStopAllSound` stops all audio.
- The VGM engine reproduces the timing and applies +1 per step to the emulated YM2612 carrier TL values. **This is an approximation, not byte-for-byte execution of the Sonic Z80 routine**: VGM doesn't contain the track RAM, the original instrument TL carrier flags, or the Sega driver. Accurate driver-level fades require running the original driver with its original sequence data.
- **Pause** is deliberately unchanged from v0.14.
- Mix defaults unchanged: PSG 75%, FM 120%, DAC 110%, Master 100%.
- VGM/VGZ importing, region and timing profiles, and diagnostics are preserved.

## Known audio issues (NOT fixed in this version)
Some FM timbres still have unintended distortion; DAC drums may have high-frequency aliasing / bitcrusher-like artifacts. These need a separate comparison against Genesis Plus GX with the same VGM and correct YM write timing, chip reconstruction and PCM data stream handling. Do not interpret this release as an audio accuracy fix.

## Local tests
C++ engine built with g++ and tested against `27 - Final Boss (Big Arms).vgz` (decompressed VGM). `test_transport.cpp`: fade duration exactly 176400 PCM frames = 4.0 s at 44.1 kHz, engine can be reloaded after stopping, and Pause remains independent. Also passed existing `test_pause.cpp`, `test_startup.cpp`, `test_audio_modes.cpp` and fade tests. The Windows UI build must be verified by the GitHub Actions Windows build workflow.

## Compile Windows EXE
Push the ZIP contents to the root of your GitHub repository. In Actions use **Build Windows Player** (`.github/workflows/windows.yml`) and download the `MegaDriveAudio-Windows-x64` artifact. Do not upload just the ZIP file into the repo.
