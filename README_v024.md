# v0.24 – Fade-out across FM, DAC, and PSG

## Fixed
- Stop/Next/Previous now apply one smooth four-second output fade to **all three audio paths**, including PSG and YM2612 DAC samples.
- No longer silence DAC and PSG immediately at fade start.
- No longer overwrite YM2612 carrier TL registers during fade, which previously caused incomplete/unequal fades, especially on instruments with different algorithms.
- Keep the original sound-driver register stream running during the fade.

## Scope and caveats
- This is a **player-level output envelope**, not yet Sonic 3K's original command-driven Z80 `zFadeOutMusic` routine. Implementing the latter requires continuous driver emulation rather than a prefetched 60-second VGM stream.
- The DAC/FM metallic or bitcrushed artifacts reported in Knuckles Theme (`0x1F`) and Big Arms have **not** been corrected here. Their cause remains to be isolated, including YM2612 multiplexing, bus clock scheduling, FIR reconstruction and clipping.
- The pre-rendered 60-second ROM track limitation remains.

## Validation
- Six portable CTest tests passed, including `all_channel_four_second_fade`, a synthetic sustained PSG tone testing that output is nonzero near the fade beginning, approximately half at two seconds, near silence at 3.9 seconds and finished after four seconds.
- Windows executable has not been compiled or listened to in this environment; build with GitHub Actions.

## Debugging audio artifacts
Compare the same Knuckles Theme excerpt with `Yamaha combined output` ON/OFF at equal output gain, with Master 65–70% and FM 100%, DAC 100%, low-pass OFF. If the effect stays with combined ON, the issue is likely upstream of the FM/DAC gain split (e.g. clocking/reconstruction), rather than the split alone.
