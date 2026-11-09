# v0.32 — Seven channel WAV stem export (Sonic 3K)

New Windows GUI button: **Export 7 Channel WAVs...** (visible below the audio sliders).

1. Open a supported Sonic 3 & Knuckles ROM (user-supplied; not bundled).
2. Select a song in the list.
3. Click **Export 7 Channel WAVs...**, enter minutes and seconds (maximum 60:00), click Export.
4. Seven stereo, uncompressed **44,100 Hz, 16-bit PCM** WAVs are created in a folder named after the song, next to `MegaDriveAudio.exe`.

Files:
- 01 - SN76489.wav (all 3 PSG tone channels and PSG noise combined)
- 02 - YM2612 FM1.wav
- 03 - YM2612 FM2.wav
- 04 - YM2612 FM3.wav
- 05 - YM2612 FM4.wav
- 06 - YM2612 FM5.wav
- 07 - YM2612 DAC.wav (physical YM2612 channel 6, which can act as FM6 when DAC disabled)

All seven files derive from **one offline Z80 playback session** and **one YM2612 instance**, not separate per-channel passes. They have identical sample counts, sample rate, and start times. The original song's own channel sequencing and loops are never reset during export. No forced fade-out.

The stem separation uses Nuked's actual time-division output slots (FM2, FM6/DAC, FM4, FM1, FM5, FM3) *before* the existing 385-tap anti-alias reconstruction filter. Each individual channel has a separate filter history. Normal playback does not perform the extra processing. PSG stem uses the existing PSG renderer. The per-stem gains match the currently displayed PSG/FM/DAC/Master sliders at export start. Optional analog lowpass and Yamaha combined diagnostic mode are intentionally not applied to the stems. The sum of stems is therefore expected to reproduce the normal mix when those optional modes are off, apart from rounding and possible sample saturation/clipping.

The export executes in a worker thread, so the UI stays responsive. Exporting a lengthy song can be CPU intensive and requires substantial disk space (seven stereo 16-bit files total about 74 MB per minute). If a same-named song folder already exists, files are overwritten. When writing to the executable folder is disallowed, export reports a file creation error rather than silently saving elsewhere.

## Validation
- Local Linux C++ build: passed for portable components, including stem renderer.
- Ten portable CTest cases passed (new `seven_stems_sum_to_reference_mix`).
- Stem sum versus reference sound over 6800 PCM frames: max absolute error **2** PCM LSBs in the synthetic test.
- Offline `stem_export.cpp` syntax compiled separately.
- **Windows GUI build and end-to-end ROM-to-seven-WAV export are not yet verified**; GitHub Actions/Windows test is required. The optional real-ROM test (`mda_test_stem_export`) checks all seven WAV lengths when `MDA_TEST_SONIC3K_ROM` is set to a user-supplied ROM; otherwise it tests invalid inputs. The project's Z80 source is downloaded by CMake from GitHub when `MDA_ENABLE_Z80=ON`, and that dependency was unavailable to the local Linux environment.

## Preservation
- Existing v0.25 YM2612 clock/reconstruction path and 44,100 Hz output remain unchanged for ordinary listening.
- Existing v0.31 fade override and continuous ROM player controls remain intact.
- No copyrighted game ROM is distributed.
