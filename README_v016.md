# v0.16 — ROM-backed audio CPU foundation (not yet a Sonic song player)

The Windows VGM/VGZ player and all existing user controls are preserved. This release adds **real cartridge-ROM access for the emulated sound bus** and a command-line ROM probe. The Z80 and Motorola 68000 CPU adapters remain **optional and experimental**; no ROM-based Sonic music playback is claimed.

## Implemented

- `SoundBus::loadRom()` accepts caller-supplied Sega Mega Drive ROM bytes. No copyrighted ROM is distributed.
- 68000-side ROM reads from the cartridge address space.
- The Z80 memory window `$8000–$FFFF` reads from the cartridge ROM using the bank latch set via nine successive writes to `$6000` (LSB first). Resetting the bus does not eject the ROM.
- `$0000–$3FFF` maps to mirrored 8 KB Z80 RAM. YM2612 ports at `$4000–$4003` and PSG at `$7F11` retain their existing paths.
- `mda_rom_probe` recognizes and inspects a user-provided ROM header and confirms sound-bus ROM access. GitHub Windows build includes `mda_rom_probe.exe` beside `MegaDriveAudio.exe`.
- `tests/test_rom_banking.cpp` verifies cartridge banking, Z80 RAM mirroring and 68000 ROM reads, plus existing bus and VGM-trace tests.

## How to run the Windows diagnostic

Open Terminal in the extracted Windows artifact directory and run:

```powershell
.\mda_rom_probe.exe "C:\path\to\your\Sonic3K.bin"
```

It reports system, game title, product number, region and 68000 reset vector. **It does not play the ROM's music yet.** The GUI VGM player works as before.

## Verified here

Compiled the portable C++ bus and diagnostic tool, passed all 3 CTest tests, and read the header of the user-provided *Sonic 3 & Knuckles Sound Test B Fade* ROM: `SEGA GENESIS`, `SONIC & KNUCKLES`, `GM MK-1563 -00`, reset vector `$000206`, read-only cartridge mapping OK. No ROM is included in this source archive.

## What is missing before original Sonic music can play

- Emulate the full enough 68000/Z80 startup and game-specific transfer of original Z80 driver code and music data into sound RAM.
- CPU interrupts/timers, bus ownership, YM2612 status/busy reads, Z80 RAM handoff, memory-mapped VDP interactions needed by sound driver.
- Execute the actual SMPS music command for Big Arms on the original driver and route timestamped chip writes **directly** into a synchronized YM2612/PSG renderer (not via the interim VGM trace).
- Compare recorded registers and audio with Genesis Plus GX before claiming fidelity. A VGM file by itself does not provide the original SMPS/CPU program.

Do not confuse this v0.16 foundation with finished original-driver playback. No changes are made to the existing four-volume mix, PAL/NTSC selection, Pause, Stop or fade functions in the GUI.
