# v0.31 — Override fade with repeat selection

- While a Sonic 3K track change is fading, selecting the **same queued track again** starts it immediately in the existing live Z80 session.
- After pressing Stop, selecting a ROM track before fade finishes interrupts the fade and starts that song immediately.
- Selecting a **different** song while a track-change fade is pending replaces the queued target; selecting that new target a second time makes it immediate.
- The Play button also triggers this when the selected track is fading after Stop (rather than toggling pause).
- The ROM command mailbox, YM2612 state, PAL/NTSC timing and 44.1 kHz sound engine stay unchanged. No added master-volume fade.
- VGM playback behavior remains as before.

**Tests**: portable `mda_test_fade_transition` includes state-transition assertions. Windows integration and actual listening require GitHub Actions and a user test. After an override, already queued PCM (tens of ms) is not discarded, avoiding an audible click; immediate means next Z80 frame, not literally zero latency.
