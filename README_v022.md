# v0.22 — Sonic 3K ROM selection inside the Windows player

- **Open ROM** reads user-provided Sonic 3K/lock-on ROM; list entries now start playback on double-click or Play.
- Uses the **original Kosinski-decompressed Z80 sound driver**, CPU emulator and existing YM2612/PSG audio engine.
- Z80 runs in an asynchronous worker, producing a **60-second in-memory VGM event stream**. No temporary VGM/WAV file is required.
- PAL/NTSC settings are passed to the Z80 driver. Volume and playback transport use existing GUI controls.
- **Prototype limitations:** the 60-second trace ends rather than extending driver execution seamlessly; initial preparation can take time; switching tracks during preparation waits for that worker to finish; Sonic 1/2 and standalone Sonic 3 are not supported by this ROM bootstrap; fade/pause currently use the GUI engine (not the game's original commands).
- Windows GitHub Action now configures with `-DMDA_ENABLE_Z80=ON` and fetches the Z80 CPU library. An Internet connection is required for configure.
- No copyrighted ROM is distributed. Supply your own legal ROM.

**Testing:** Build Windows x64 Action; open a Sonic 3K lock-on ROM; double-click `Music 0x01` and wait for render. Compare with the v0.21 probe's known-good Angel Island Act 1 result.
