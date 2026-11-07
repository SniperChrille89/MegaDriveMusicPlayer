# v0.28 — Original Sonic 3K fade command / no progress bar

* Completely removes the timeline/progress bar and its seek UI.
* ROM Stop, Next, Previous, and selecting a new ROM track inject `$E1` through the actual Z80 mailbox (`$1C0A`), rather than calling the mixer's master-volume fade.
* Keeps running the original Z80 driver, emitting chip writes, until `zFadeOutTimeout` ($1C0D) has counted down following the command; plays the result, then stops/restarts.
* Shortens ROM stream prefetch to 1-second chunks to reduce delay between UI command and the audible original fade. This is a latency/safety trade-off, not guaranteed sample-perfect timing.
* Non-ROM VGM playback retains its existing player-level fallback fade.
* Does NOT change v0.25's 44.1 kHz FM/DAC renderer.

**Test limitations:** Needs Windows x64 build and audio listening test; fade behavior may depend on the actual ROM driver and the driver's internal fade counters. No guarantee of bit-for-bit original fade until compared to real hardware/RetroArch.

## How to test
1. Build the Windows app with `-DMDA_ENABLE_Z80=ON` and run against a user-supplied Sonic 3K ROM.
2. Start a long sustained note, press Stop and check that chip-level stepped fade is audible (rather than uniform post-mix attenuation).
3. Check that PSG and DAC also terminate, and that Next waits for fade completion before beginning the next song.
4. Verify the progress/timeline controls are entirely absent.

The original driver fade duration is determined by the Z80's own registers and regional frame rate. This release has **not** been audibly verified on Windows, and the new short streaming buffers may need adjustment if output underruns.
