# v0.25 — Native-slot YM2612 reconstruction (44.1 kHz)

## Goal
Diagnose and reduce the metallic/ring-modulation-like side tones reported on Sonic 3K Big Arms/Final Boss, Launch Base bass, and Knuckles Theme `$1F` DAC samples. This version changes no Z80 driver instructions, ROM song data, VGM timing, track IDs, slider defaults, or sample rate.

## Change
The previous audio renderer performed 8x oversampling by averaging roughly 3–4 native YM output clock slots per intermediate sample, and then filtering these intermediate samples. The YM2612 output is time-division multiplexed (24 output slots per frame), so short-block averages can be phase dependent.

This experiment preserves **every `OPN2_Clock` output tick**, including original YM2612 ladder behavior, labels DAC slot timing before each chip tick, and uses a 385-tap Blackman-windowed sinc low-pass at the chip output rate (~1.27 MHz) prior to the 44,100 Hz sample point. The 18 kHz low-pass has modeled attenuation of about -108 dB at 53 kHz (native 24-slot multiplex rate), with some loss above 15 kHz. FM and DAC gain sliders still work separately. Combined mode continues to sum both paths.

The original host's FM/DAC split and the chip DAC-bit latching semantics still require careful listening; a clean ROM-based A/B recording from RetroArch has not been supplied or compared. **No claim that the reported audible problem is solved** until listening verification.

## Verification
- Linux CMake build: successful (`MDA_ENABLE_Z80=OFF` — the Windows Z80 integration requires a GitHub FetchContent dependency that could not be downloaded here).
- Six CTest tests: passed.
- 8-second Big Arms VGM-to-WAV render: generated 44.1 kHz stereo PCM, peak 10,138 out of 32,767, no clipping.
- Windows UI build and Sonic 3K ROM realtime playback: not verified here.

## Listen for
1. Launch Base bass: unwanted double pitch / beat should change or disappear if slot averaging caused it.
2. Knuckles Theme `$1F`: DAC bitcrusher/ring-like side-tones.
3. Big Arms / Final Boss: high FM chords and lead.

Keep Yamaha Combined Output OFF initially to preserve the existing channel balance; if artifacts persist, switch it ON to test the split. For reference comparisons, use analog low-pass OFF, normal headphone output, master volume below digital clipping, and unchanged PAL/NTSC.
