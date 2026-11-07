# v0.26 — Persistent Sonic 3K Z80 ROM playback (experimental Windows integration)

## Major change

Previously the Windows app generated a fixed 60-second VGM file in memory, so Sonic 3K music stopped at 60 seconds. v0.26 holds a single Sonic 3K Z80 CPU and its original sound driver state alive for the selected song and extends the music trace in short blocks. The chip-synthesis engine keeps its YM2612, PSG, DAC and 385-tap FIR state **between** these extensions. The continuous original Z80 driver should therefore repeat its own individual SMPS channel loops indefinitely, with no forced global rewind.

- The first block is prepared by the background Z80 worker, and further 12-second blocks are generated before playback reaches the current buffer end.
- No WAV/VGM files are written to disk. The intermediate command stream remains in memory.
- The renderer and output sample rate remain **the validated v0.25 version at 44,100 Hz**, unchanged except for the append API.
- Stop and track switching cancel the current stream selection. The old 4-second mixed-audio fade is still present; replacing it with actual Z80 `$E1` / stop instructions is NOT part of this version.
- The progress timeline shows **elapsed time and buffer horizon** for ROM streaming. It deliberately does not show a fabricated musical loop boundary.
- ROM seeking is temporarily disabled. True ROM seeking requires coordinated CPU/RAM/banking + audio chip snapshots; seeking just inside a trace without restoring the original CPU would produce divergent audio.
- VGM playback and VGM seeking remain available.

## Test status

- Portable C++ compilation: passed.
- 7 CTest tests: passed, including `rom_stream_append_without_audio_reset`.
- Regression verifies *sample-identical* audio before and after an appended VGM segment compared to continuous VGM (PSG fixture).
- Sonic 3K driver stream code passes syntax compilation against a mock Z80 API, **not a live real-Z80 integration test**.
- Windows/MSVC GitHub Actions build and real Sonic 3K ROM listening still require user verification.

## Build

The GitHub workflows use CMake with `-DMDA_ENABLE_Z80=ON`; the GitHub Actions runner downloads `superzazu/z80` as before. Run **Build Windows x64**; use the generated `MegaDriveAudio.exe`.

## Outstanding work

1. Genuine original-driver fade, pause and stop messages.
2. True loop-boundary analysis for the timeline and a bounded circular position display.
3. ROM seeking with Z80 + sound chip checkpoints.
4. Robust low-latency buffering on slow computers, optional recording.

This version does NOT claim to detect exact intro/loop positions or to have been built on Windows.
