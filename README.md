# Mega Drive Music Player v0.10 — Yamaha diagnostic mode and optional low-pass filter

This is a **test-focused update**, not yet a verified Genesis Plus GX-equivalent audio engine.

## Changes

- **Yamaha combined output** checkbox (off by default): uses one FM slider gain for the full original Nuked signal (including DAC), so you can compare the *complete YM2612 signal* against the experimental separate FM/DAC level controls. When checked, **DAC slider is intentionally bypassed**. This mode lets us diagnose any coloration caused by splitting the multiplexed output. The mandatory 8x anti-alias filter is unchanged.
- **Optional low-pass filter** checkbox (off by default) with adjustable strength from 5% to 95%, default 45%. This is a separate output color filter, not a substitute for correct chip timing or for the anti-alias filter. 5% is relatively light; 95% is substantially warmer. The control is not a calibrated replica of a particular Model 1 motherboard revision.
- The desktop window is taller to display both audio diagnostics below the four existing volume sliders.
- Keeps **PAL / NTSC and timing profiles**, **PSG/FM/DAC/Master sliders**, four-second track-change fade and Sonic-inspired pause.

## Listening test

1. Use **the exact same VGM** in our app and Genesis Plus GX, preferably at the file's original timing (avoid guessed PAL conversions).
2. Set **PSG 100, FM 100, DAC 100, Master 100**. Turn the optional low-pass **off**.
3. Listen with **Yamaha combined output off**, then **on**. At equal FM/DAC levels the resulting digital stream must match exactly; this is tested. To diagnose the split, try FM 160 and DAC 100 with combined off, then combined on. In the latter case DAC deliberately follows FM.
4. Then enable low-pass at 45%, comparing tonal balance rather than trying to hide erroneous bitcrushing.
5. If the DAC sample rate or FM bass pitch is wrong in both modes, the root cause is upstream (VGM DAC scheduling, chip clock, or note/register timing) and **not** the separated gain path or optional filter. Report which VGM file and whether the issue occurs in both modes.

## Current limits

- The C++ render core and controls have been edited, but **Windows EXE cannot be compiled in this Linux environment**. Build via the included GitHub Actions workflow.
- The VGM decoder lacks support for some v1.60 streaming commands, particularly the 0x90–0x95 DAC stream family. Such files may not play correctly; do not interpret those failures as accurate emulation.
- YM2612 address/data writes are queued and may be delayed relative to recorded DAC event timing. This issue **has not been fixed or objectively compared** against Genesis Plus GX yet. The diagnostic mode does not solve it.
- The optional low-pass is a simple one-pole filter and is not lifted from Genesis Plus GX. Nuked remains under its LGPL license; satisfy redistribution requirements if publishing binaries.

---

# Mega Drive Music Player v0.9 — console-style Pause / natural FM release

A Windows C++ VGM/VGZ player using Nuked OPN2. Keeps v0.8's PAL/NTSC region and independent timing profiles, 8x filtering, the four volume sliders (PSG/FM/DAC/Master), and **4-second track-change fade**.

## New in v0.9: Start-button style Pause

- **Pause is immediate for the VGM sequencer.** No more four-second fade on Pause.
- All six FM voices receive a YM2612 **key-off** (register 0x28), but the Nuked chip keeps clocking while paused for **up to 0.50 seconds**; audible FM release envelopes and hardware ladder characteristics remain. Some instruments end sooner based on their release rate.
- PSG is muted immediately (it does not have the YM2612's release envelope). PCM/DAC is disabled immediately for the paused tail.
- After 0.50 seconds the audio output is muted and VGM time is still frozen. This is a short **natural release**, not a digital fade or echo plug-in.
- **Play** restores the DAC enable mode and advances the VGM from the exact frozen position, **without retriggering the note interrupted by Pause**. New FM key-on and PSG volume events in the VGM determine which channel returns first.
- **Next / Previous track still uses the independent 4.00-second hardware fade.**

This is a Sonic 3K-inspired behavior approximation for existing VGM register streams, **not** an exact emulation of every ROM's Z80 sound driver pause commands. A chip's output history is not the same as the game's original sequencer state. The implementation uses YM2612 key-off followed by a maximum release window; no artificial delay is added.


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
- `test_engine.cpp` is the existing engine smoke test; `test_fade.cpp` and `test_buses.cpp` test existing behavior. `test_pause.cpp` checks the 0.50-second YM release tail, frozen VGM position, silence afterward, and resume.
- C++ engine, profile, pause, and fade tests were compiled and run locally on Linux. The pause/fade tests also passed using an actual Big Arms VGM (not included in this package). **The Windows GUI has not been compiled here**, because this environment does not include a Windows cross compiler. The bundled `.github/workflows/windows.yml` builds the Windows EXE through GitHub Actions.

To update your repository, extract **the contents** of the ZIP to the root of your local `MegaDriveMusicPlayer` checkout, replacing existing files, then Commit and Push in GitHub Desktop. In GitHub Actions, use **Build Windows Player** and download the `MegaDriveAudio-Windows-x64` artifact. Keep `unvgz.ps1` alongside the EXE.

## Known limitations

- The PAL conversion is an approximation of game-driver timing, not a complete sound-driver emulator.
- Source-rate metadata may be absent or inaccurate.
- PSG, analog filtering, FM/DAC balance and certain VGM DAC streaming commands still require development and audio comparison to real hardware.
- 4-second track-change hardware fade and 0.5-second Sonic-inspired pause tail are approximations; neither is claimed to be bit-identical to the original Sonic 3K driver.
- Nuked OPN2 is LGPL 2.1+; ensure library license and redistribution/relinking requirements are followed.
