# Z80 Windows linker fix

The GitHub Actions build failed with LNK2019 for `z80_init` and `z80_step` while compiling `mda_test_z80` and `mda_z80_generate_vgm`.

Cause: `z80.c` is compiled as C, but its declarations were included from `z80_driver.h` as C++ without `extern "C"`.

Fix: wrap `#include "z80.h"` in an `extern "C"` block for C++ callers. This ensures `z80_init` and `z80_step` get C linkage, matching the functions compiled in `z80.c`.

The playable Windows GUI and existing audio code are untouched. The compiler warnings concerning `UNICODE` and `HMENU` are not the cause of this failure.

To use: replace the project files, commit/push, then run the **CPU Audio Engine (Z80)** workflow. Its success should produce the `Real-Z80-Audio-Demo` artifact. The GUI EXE is produced by **Build Windows Player** (separate workflow).

This repair is source-verified against the GitHub build log. It is not yet confirmed by rerunning Microsoft's Windows CI.
