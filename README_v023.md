# v0.23 – Intro / Loop timeline and seeking

The player has a custom clickable/drag-enabled timeline between the playlist and transport controls. It shows playback position and total recorded duration. For VGM/VGZ files with a valid loop offset the purple vertical tick marks the original loop entry; for Sonic 3K ROM audio the 60-second CPU trace has no loop data, so the UI correctly reports **Loop: not identified** instead of inventing a point. Audio position seeking uses fully reproducible YM/PSG state checkpoints (5s cadence when played), resetting the waveOut buffer to discard stale audio. Seeking ahead of the currently-rendered segment can take time because the emulated chip state must be advanced correctly. ROM audio still has the existing 60-second cap; this version does **not** auto-detect game-driver loop structure.

To build: use the existing GitHub Actions Windows workflow (`MDA_ENABLE_Z80=ON`). Source code contains no copyrighted game ROM.
