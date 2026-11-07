# v0.21 – ROM Z80-to-WAV listening probe

This is an **audio diagnostic**, not yet the ROM music player in the GUI.

It runs the original Kosinski-decompressed Sonic 3K Z80 driver for six PAL seconds,
traces its actual YM2612 and PSG writes, converts the trace into VGM and renders
it into 16-bit 44.1 kHz stereo WAV through the project's existing Nuked-based engine.

## Test on Windows

After GitHub Actions → **CPU Audio Engine (Z80)** succeeds, download **Real-Z80-Audio-Demo**.
Place a personal Sonic 3 & Knuckles `.bin` ROM beside the extracted programs.
In Explorer address bar enter `cmd`, then run:

```
mda_s3k_z80_probe.exe Sonic3K.bin test.vgm 01 test.wav
```

Open `test.wav` in any audio player. If it is noisy, silent, or not a song,
share the console log and WAV. That is useful diagnostic evidence; **we have not
verified musical playback yet**. The test command `01` is not confirmed to
select a particular song. The first six seconds include boot time.

A standalone VGM-to-WAV tool is also included:

```
mda_vgm_to_wav.exe test.vgm replay.wav
```

## Important limitations

* FM/DAC quality is inherited from the v0.20 playback engine; known artifacts remain.
* Driver startup, music ID, timing, DAC bus-busy behavior and PAL interrupts still need validating against hardware.
* The generated VGM stores the real CPU-emitted register trace, not the original music program.
* No game ROM is distributed.
