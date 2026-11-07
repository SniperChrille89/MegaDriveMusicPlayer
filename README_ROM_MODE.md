# ROM mode: integrated playlist (v0.18)

This version displays Sonic 3/S&K music command IDs inside the **main playlist**, not a separate window. VGM/VGZ playback remains available.

**Important limitation:** Selecting a ROM track currently does **not** run the Sonic 3K sound driver. The Z80 test backend exists, but no ROM audio boot/driver dispatch has been completed. Playback of ROM tracks is disabled with an explicit explanation rather than silently playing wrong data.

To implement actual ROM audio the next milestone must: (1) select a specific compatible ROM, (2) initialize Mega Drive 68K and Z80 sound RAM, (3) load/execute the correct Z80 driver, (4) deliver music commands into the driver's mailbox, (5) schedule interrupts and YM2612/DAC/PSG writes on shared hardware time, (6) connect the result to the player's PCM output. That must be tested with an actual ROM and validated against the original Sound Test.

Build Windows using the existing .github/workflows/windows.yml workflow. Linux CMake tests can check ROM identification but cannot compile the Win32 UI.
