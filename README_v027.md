# v0.27 — Sonic 3K Sound Test names + MSVC build fix

- Replace generic `Music 0xNN` track labels with the Sonic 3 & Knuckles Sound Test song titles, retaining the hex command ID in brackets. Source: [sonicretro/skdisasm sonic3k.constants.asm](https://github.com/sonicretro/skdisasm/blob/master/sonic3k.constants.asm).
- No ROM data is included. The detected Sonic-family candidate profile retains the existing limitation: the named table corresponds to the **Sonic 3 & Knuckles** Sound Test; other ROM variants may differ in arrangement, IDs and availability. This does not implement separate Sonic 1/2/3-alone playback.
- Fix the MSVC failure due to capturing `InitialSeconds` and `MoreSeconds` in asynchronous lambda expressions by using explicit values for the worker captures (`15u` and `12u` respectively). The ROM streaming and YM2612 playback engine are otherwise unchanged.
- Test `mda_test_rom_catalog` verifies representative titles and IDs.

## Tests

Portable CMake/CTest can test catalog mappings; Windows GitHub Actions **Build Windows x64** still needs validation. Playback continues to be the experimental v0.26 continuous Z80 stream.
