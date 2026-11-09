# v0.37 — Original Sonic 3 & Knuckles fade, with paced native Z80 steps

**Purpose:** restore the authentic Sonic 3K Z80 fade (`$E1`) instead of changing
all four YM2612 FM operators' Total Level values from the Windows host.

The actual decoded Sonic 3K driver (in a user-supplied ROM) initializes
`$1C0D` (`zFadeOutTimeout`) to `$28` (40 steps), and sets the fade delay
at `$1C0E/$1C0F` to 6. The previous 0.34 code watched **only** `$1C0D`;
v0.35/0.36 substituted an external per-register fade and altered instrument
modulation. Both approaches have been removed from the **ROM playback path**.

`OriginalFadeClock` now drives only the native sub-step frame-delay RAM
`$1C0F`, allowing the original Sonic 3K code to perform each FM operator
adjustment. No synthetic YM/PSG/DAC writes are introduced. The original
`$E1` operation remains responsible for PSG and DAC behavior.

- STOP: 5 seconds; next music track / previous / direct selection: 3 seconds.
- Selecting the pending track again: immediate original music command.
- Native 40 driver steps are scheduled over the requested 50/60 Hz frames.
- Audio renderer, YM2612 filter, continuous Z80, export of 7 WAV stems and
  working Windows export dialog are kept intact from the v0.34 baseline.

**Testing limits:** 11 portable tests pass, including a deterministic simulation
of the original 40-step timing at 3/5s PAL+NTSC, with simulated previous loop
histories. The live Z80 integration and audible fade after 5+ loops have **not**
been proven in this container: the project's external Z80 dependency cannot
be downloaded here, nor can the Windows GUI be run here.

**Important:** This fixes independent, exact scheduling of the original routine
but does not conclusively identify the previously reported loop-dependent fault.
If the loop-dependent bug persists in the Windows build, inspect the Z80 RAM
variables `$1C0D`, `$1C0E`, `$1C0F` and emitted FM register writes at fade start.
