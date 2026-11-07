# v0.19 – Sonic 3K Z80 driver bootstrap probe

This is a **diagnostic development build**, not Sonic 3K ROM music playback yet.
The current GUI still plays VGM/VGZ. It does not claim that selecting a ROM track triggers audio.

## What changed
- Added ROM bootstrap pattern detection for the original Sonic-style Z80 sound driver.
- Added `mda_s3k_rom_scan` to check a user-supplied ROM without executing anything.
- Added `mda_s3k_z80_probe` (built with `MDA_ENABLE_Z80=ON`) to execute up to 500,000 Z80 CPU cycles from the ROM-derived bootstrap and report actual YM2612/PSG port writes.
- Added optional VGM export of captured CPU writes **for diagnosis only**.
- Extended the separate GitHub CPU test workflow to upload the new `.exe` diagnostics in the `Real-Z80-Audio-Demo` artifact.

## Local verification
- Five CTest tests pass with the portable C++ backend.
- `mda_s3k_rom_scan` detects the bootstrap in an existing user-supplied Sonic 3K ROM at hexadecimal ROM offset 0xF6962 (7152 initial RAM bytes).
- The Z80 execution probe itself has **not been executed in this environment** because the external Z80 CPU library is unavailable locally. The GitHub Windows workflow builds it and can reveal further integration errors.
- No ROM bytes are included in the distributed project or artifacts.

## Testing via GitHub Actions
1. Replace repository source files with this project's files; Commit and Push.
2. Open Actions -> `CPU Audio Engine (Z80)`; run the workflow.
3. If the job succeeds, download the `Real-Z80-Audio-Demo` artifact.
4. Extract the artifact and open PowerShell in its directory. To locate the original driver, run:
   `./mda_s3k_rom_scan.exe "C:\\path\\to\\Sonic3K.bin"`
5. To attempt CPU execution and generate a diagnostic VGM trace, run:
   `./mda_s3k_z80_probe.exe "C:\\path\\to\\Sonic3K.bin" "driver_bootstrap_trace.vgm"`
6. Report YM/PSG write counts and CPU PC shown in terminal. A non-zero count does not automatically prove a song can play.

## What remains
- Verified Sega 68000-to-Z80 driver initialization, memory load and Z80 reset/bus sequence.
- Correct interrupt/frame scheduling (V-int, PAL/NTSC).
- Original Sonic 3K music command injection and runtime data layout.
- Correct ROM bank layout and DAC PCM timing.
- Route real CPU writes directly to the *same* live YM2612/PSG sound engine used by the Windows application.
- Play a named track in the main player and test its fade/stop, without the VGM-file intermediary.

**Note:** A normal VGM is not a ROM sound-driver program; this diagnostic reads an actual ROM to move toward real driver playback.
