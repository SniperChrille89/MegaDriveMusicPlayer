# v0.34 — WAV export dialog creation fix

Fixes the Windows `Could not create the WAV export window.` error.

Root cause: `exportDialogProc` forwarded `lParam=0` to `DefWindowProcW` for all unhandled messages. Windows needs the original `CREATESTRUCT` pointer from `WM_NCCREATE`; otherwise window creation can fail. The window procedure now takes the `LPARAM lp` argument and passes it unchanged to `DefWindowProcW(hwnd,msg,wp,lp)`.

No modifications to Z80, YM2612, DAC, PSG, WAV encoding, sample rate, export layout, or playback logic.

Build Windows x64 with `-DMDA_ENABLE_Z80=ON` as before, then select a ROM track and click **Export 7 Channel WAVs...**.

This fix was checked in source and portable tests; the Windows GUI itself still requires verification on the user's PC.
