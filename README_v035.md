# v0.35 – loop-independent chip-register fade (PAL/NTSC)

Sonic 3K ROM playback now uses an independently timed fade on the **live chip
register stream**. It does **not** restart the song, reset YM2612, regenerate
VGM audio, fade the final Windows PCM mix, or alter the game's Z80 RAM.

- **Stop:** 5 seconds (250 frames PAL / 300 frames NTSC).
- **Next, Previous or selecting another track:** 3 seconds (150 PAL / 180 NTSC), with the next song dispatched on the final fade frame to avoid a gap.
- **Selecting the queued song again:** cancels the fade and dispatches the new
  song ID at the next Z80 frame. Selecting a track during a Stop fade does the
  same. All of this keeps the same live Z80/YM2612/PSG session.
- The chip-register fade is reset *only* by user commands, not by song loops. A restarted fade preserves the current attenuation so the old song does not jump back to full volume.
- FM channel attenuation modifies Total Level registers, PSG uses chip volume
  commands and DAC values are attenuated about the YM2612 midpoint before the
  existing v0.25 reconstruction filter. There is no post-DAC master fade.

This is **not identical to the original Sonic 3K Z80 fade routine**. The old
native `$E1` path was reported to become unreliable after multiple song loops,
so this uses a deterministic independent register-level envelope to fulfill
fixed-duration playback requirements. The original Z80 driver still executes
music continuously and unmodified. FM, PSG and DAC fade shapes may differ
from the game's built-in fade and should be checked against hardware.

The 44,100 Hz YM2612 reconstruction and 7-stem export implementations are
not modified. Existing VGM fade behavior is unchanged.

## Verification

Portable CMake/CTest: 11/11 tests passed, including tests for deterministic
3/5-second durations independent of prior frames, register-level FM/PSG/DAC
attenuation, and preservation of DAC address latching across frame boundaries.
`src/hardware/rom_audio.cpp` C++ syntax also checked with an isolated test-only
Z80 header stub. The **full Windows GUI**, real Z80 runtime, and audible
late-loop transitions have **not** been verified here; build in GitHub Actions
and listen in Windows before treating this release as stable.
