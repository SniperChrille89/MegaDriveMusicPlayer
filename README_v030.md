# v0.30 — Seamless in-session Sonic 3K song changes (experimental)

The Sonic 3K ROM player no longer tears down its Z80 driver and resets the YM2612/PSG when a user changes from one ROM track to another. Instead:

1. Next/Previous/track selection sends Sonic 3K's native `$E1` fade command, if a fade is not already in progress.
2. The selected next song's music ID is queued in the *same* persistent `Sonic3KStream`.
3. Once the original driver's fade timeout has been observed and has two or fewer remaining steps, the next song command is placed in the Z80 music mailbox for the next video frame.
4. The resulting Z80 register stream is appended to the existing player without resetting YM2612/PSG, resampling filters, or the Windows audio device.
5. Stop still issues `$E1` and ends the session when the original driver signals fade completion; it does not queue a next song.

No artificial crossfade or post-mix master-volume curve has been introduced. v0.25's native YM2612 reconstruction and the 44,100-Hz output are unchanged. The progress bar remains removed. Sonic 3K track names and indefinite playback are preserved.

## Verification

* 9/9 portable CTest tests pass (including `original_z80_seamless_transition_condition`).
* The ROM audio source passes a C++17 syntax-only check using a local Z80 header stub. This is not a functional ROM test.
* Windows x64 build and actual in-game-like sound transitions have **not** been verified in this environment. Build in GitHub Actions (`Build Windows x64`).

## Important limitation

The handoff is currently scheduled when `zFadeOutTimeout` reaches <=2; this is a deliberate approximation to an audible seamless game-style transition, **not yet proven to be the exact frame used by Sonic 3K's game code**. A song change during an abnormal fade state may still need diagnostics. In particular, the previously reported issue where Final Boss sometimes fails to attenuate after several loops is **not confirmed fixed** by this change.

Please test switching between Final Boss `$19`, Knuckles Theme `$1F`, and another track after short and long playback, noting whether an audible silence or clicks remain. Do not place ROM files in the GitHub repository or package.
