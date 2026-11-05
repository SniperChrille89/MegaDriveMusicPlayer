# Mega Drive Music Player v0.6 — PAL / NTSC + four-channel mixer

Windows C++ source using Nuked OPN2 (YM2612) and an experimental Sega PSG emulator.

## v0.6: Sonic 1 European PAL timing

- Visible **PAL 50 Hz** / **NTSC 60 Hz** radio buttons at the **top of the window** (default NTSC, preserving the previous player behaviour for NTSC rips).
- Switch modes **during playback**. The VGM register-event timeline is converted using the rate at header offset `0x24`, when it contains 50 or 60 Hz.
- A 60 Hz VGM played at PAL 50 Hz advances its *register event sequence* at **5/6 speed**. Therefore a 60-second source track takes approximately 72 seconds in PAL, like an original uncorrected 50 Hz Sonic 1 sound driver. The YM2612 and PSG continue rendering at the output sampling rate, with a small PAL-vs-NTSC hardware oscillator adjustment (~0.9%) rather than changing the pitch by 16.7%.
- A file *already recorded at 50 Hz* plays unchanged in PAL mode (no double-slowdown); selecting NTSC speeds up its event timeline 6/5.
- If the VGM rate is **zero or not 50/60**, the player clearly displays **"source unspecified (60Hz assumed)"** and uses 60 Hz as the fallback for its conversion. You need a correctly tagged source for reliable automatic conversion; some VGM rips do not specify their source rate.
- The status line reports the current **playback region** and **file's declared source rate**.
- **Limitation:** converting recorded register timings is a *compatibility approximation* for an uncorrected 50 Hz game. It cannot perfectly recreate game-specific PAL sound driver changes or convert every VGM into a genuine PAL hardware capture. In particular, files from games that already compensate for the PAL update rate should be played at their natural captured rate.

## Existing features retained

- PSG, FM, DAC, Master independent volume sliders, 0–200%.
- Mixer defaults: PSG 100%, **FM 160%**, DAC 100%, Master 100%.
- 4.00-second FM operator-level fade when switching tracks or pausing (PSG and DAC mute immediately; this is not yet identical to Sonic 3K's native routine).
- VGM/VGZ open, scrolling track list, Play/Pause, previous/next.

## Build the Windows EXE on GitHub

1. Extract this ZIP **into the root** of your cloned `MegaDriveMusicPlayer` repository, replacing previous files. Preserve `.github/workflows/windows.yml` exactly as a nested folder/file.
2. Commit and push in GitHub Desktop.
3. Go to **Actions > Build Windows Player**. A push to `main` also triggers a build. Download `MegaDriveAudio-Windows-x64` from a successful run's **Artifacts**.
4. Extract the artifact and run `MegaDriveAudio.exe`. Leave `unvgz.ps1` beside the EXE for VGZ loading.

Alternative: Visual Studio 2022 + CMake: `cmake -S . -B build -A x64` and `cmake --build build --config Release`.

## Verified locally

- The C++ audio engine builds and renders Sonic 3 & Knuckles Big Arms VGM for 20 seconds without clipping at the default mixer settings.
- Synthetic rate-tagged VGMs were used to verify 60->50, 50->60, natural PAL and natural NTSC timing. A 44,100-sample / 1-second 60-Hz sequence plays for ~52,922 samples in PAL and ~44,101 in NTSC, with a one-sample scheduling boundary.
- **The Windows GUI itself has not been compiled or run in this environment**; the GitHub Windows workflow is the required build verification.

## Known limitations

- PSG remains an experimental emulator; chip sound balance is not final.
- VGM DAC streaming commands `0x90..0x95` are not implemented; traditional `0x80..0x8F` streams work.
- Pause uses a fade rather than perfectly implementing Sonic game's original Start pause.
- 200% gain combinations can clip; turn down Master if necessary.
- Nuked OPN2 is LGPL 2.1+; see `NUKED-LGPL-2.1.txt`. Distributor responsibilities include sharing the library source and observing required library relinking conditions.
