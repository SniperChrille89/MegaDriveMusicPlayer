# v0.20 – Sonic 3K: korrekt Z80-avkodning från ROM

**Viktigt:** Sonic 3 & Knuckles lagrar själva Z80-drivrutinen och dess datatabeller som **två Kosinski-komprimerade block**. 68000-rutinen `SndDrvInit` packar upp dem till Z80 RAM `$0000` respektive `$1300`, kopierar standardvariabler till `$1C00`, sätter PAL-flagga och släpper Z80-reset.

Den äldre v0.19 letade efter Z80-headern som en rå signatur i ROM. Det hittade **komprimerad data**, inte körbar Z80-kod. Därför sprang PC iväg till `$B794` och inga registerskrivningar kom.

Denna version läser 68000-initialiseringens ROM-pekare och avkodar båda Kosinski-strömmarna med korrekt 16-bitars descriptor-prefetch. Skapar en riktig 8192-byte Z80 RAM-bild. Exempel med den användartillhandahållna ROM som testats: **4459 byte program + 2115 byte ljudtabeller**, startpekare `$0085` med `LD SP,$2000`.

**Testat:** C++-dekomprimering, ROM-pekare, RAM-layout och unit tests på Linux. **Inte ännu verifierat:** Z80-körningen i Windows efter avkodning, musikkörning genom ROM, DAC-kvalitet eller ljudutmatning. Testprogrammet försöker nu en riktig musikbegäran och Z80 VBlank-avbrott. Z80-testet på GitHub är fortfarande diagnostik, inte en fullständig ROM-spelare.

## För Z80-test

`mda_s3k_z80_probe.exe Sonic3K.bin test.vgm 01` avkodar båda delarna, startar originalets Z80-kod, skickar musik-ID `$01` till `$1C0A`, simulerar 120 PAL-VBlank-avbrott och exporterar de resulterande chipkommandona till `test.vgm`. Det kan fortfarande misslyckas med riktig uppspelning; testet visar om något lämnar ljudportarna.

Sonic 3K-disassembly: `sonicretro/skdisasm`, `SndDrvInit` (68k) och `Z80 Sound Driver.asm`. Inga spel-ROM:ar distribueras i ZIP-filen.
