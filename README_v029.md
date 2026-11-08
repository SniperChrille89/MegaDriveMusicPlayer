# v0.29 — Low-latency original Sonic 3K Z80 command timing

* Preserve the user-approved v0.25 YM2612 reconstruction and 44,100 Hz audio.
* Replace one-second Z80 generation tasks with `advanceFrames(1)` tasks: 20 ms of PAL or 16.67 ms of NTSC music per task.
* Pre-generate four frames when a song first starts (~80 ms PAL / ~67 ms NTSC).
* Keep a roughly 60 ms ahead-of-playback threshold (PAL-based) and refill frame-by-frame.
* Reduce Windows `waveOut` queue from 4 x 1024 to 4 x 512 stereo frames (~46 ms).
* Sonic 3K `$E1` still controls native fade. Stop/Next/Previous now reach the original Z80 driver at the next emulated game frame, rather than up to one second later.
* On rare CPU overruns, pad with silence rather than advancing the FM/PSG synthesis timeline past the available Z80 trace.
* Progress bar remains absent. Music names and infinite driver-controlled looping remain.

## Limitations

This is lower latency, **not** cycle-perfect immediate response. Video-frame timing and the OS playback device introduce latency; the smaller buffers can expose underruns on slower machines. Stop/Next still waits for the original sound driver's fade completion. End-to-end Windows audio latency needs a listening test.

## Test

Build with `-DMDA_ENABLE_Z80=ON`. Start Sonic 3K and press Stop at different points in Knuckles Theme and Robotnik Boss. Compare fade start and length to your modified in-game Sound Test. Check for stuttering while playing continuously for several minutes.
