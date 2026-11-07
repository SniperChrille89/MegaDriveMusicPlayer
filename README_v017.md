# Mega Drive Music Player v0.17 — ROM music catalog / CPU backend foundation

**This is NOT yet a fully playable ROM-driven music emulator.** It is a testable milestone toward that goal; the VGM/VGZ player remains the working audio path.

## New in this build

- **Open ROM (preview)** in the existing Windows application accepts a user-owned uncompressed Mega Drive `.bin` / `.md` cartridge image.
- Reads the Sega cartridge header (domestic / overseas title, product code, region) and recognizes the Sonic 3 / Sonic & Knuckles game family.
- Opens a separate ROM-song-command window showing command IDs `0x01..0x32` and `0xDC` associated with the Sonic 3K family. They are *candidate command slots*, not verified extracted tracks or guaranteed song titles for every ROM revision.
- Double-clicking one of these entries clearly explains that the original-driver playback is **not connected yet**, rather than silently playing a different VGM file.
- Adds a standalone `mda_rom_catalog` module that runs on both Windows and Linux and has CTest coverage, including a test with a user-supplied Sonic 3K ROM.
- Existing Windows VGM/VGZ playback and mix/region/transport behavior retained without changes. Optional Z80/68000 and cartridge sound-bus modules from v0.16 retained.

## Testing done

CMake built on Linux and all 4 portable unit tests passed, including sound bus, ROM banking, VGM trace, and ROM catalog. The ROM catalog module also recognized the existing modified Sonic 3 & Knuckles test ROM (header title `SONIC & KNUCKLES`, 51 candidate command entries).

Windows GUI *has not* been compiled/tested in this Linux environment. Use the already-working GitHub Actions **Build Windows Player** workflow to build `MegaDriveAudio.exe`. No game ROM or commercial game music is shipped with these sources.

## Missing for real ROM music playback

1. Real Z80 and Motorola 68000 initialization, RAM transfer, bank switching, interrupts, and sound driver discovery for an identified ROM revision.
2. Execute the game's *actual* sound driver and pass sound-test commands to it; do not attempt to treat a list of command IDs as playable audio.
3. Synchronize CPU cycles with YM2612/PSG timing and DAC reads/writes, and verify accuracy against gameplay / reference recordings.
4. Read music metadata/titles per supported game profile. Other games need other profiles and may use different sound drivers.

There is **no promise** that opening a ROM in v0.17 will produce audio. It is a verified ROM-reading/catalog milestone while development proceeds toward real-driver playback.
