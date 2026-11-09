# v0.36 — Sonic-style FM-only fade

- **Stop:** five-second fade on YM2612 FM1–FM5.
- **Track changes:** three-second fade on YM2612 FM1–FM5.
- **SN76489 PSG:** all four voices muted on first fade frame and held muted.
- **YM2612 DAC:** channel 6 PCM data forced to midpoint 0x80 on the first fade frame and each subsequent DAC sample; no audible DAC fade.
- **Fade override:** selects next song using the existing live Z80 session and restores normal processing.
- **Important:** FM attenuation remains the deterministic register-level envelope from v0.35. This is not a claim of bit-exact Sonic 3K's original fade routine.
- YM2612 output reconstruction, WAV stem export, 44.1 kHz, and ROM driver are unchanged.

**Tests:** `cmake -S . -B build -DBUILD_TESTING=ON && cmake --build build -j && ctest --test-dir build --output-on-failure`.
