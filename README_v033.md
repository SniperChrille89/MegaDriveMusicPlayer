# v0.33 — WAV Export Dialog Fix

Fixes a silent return from the WAV export button when the list selection is temporarily absent. Uses the currently playing ROM track as fallback; otherwise displays an actionable warning.

Also checks Win32 export dialog class registration and window creation, displays errors instead of silently ignoring them, and raises the export dialog to the foreground. The chosen track is captured when the dialog opens so playback selection changes don't silently switch what gets exported.

No changes to Z80, YM2612, PSG or WAV stem rendering.

Build on Windows via GitHub Actions: **Build Windows Player**, then launch MegaDriveAudio.exe, open a Sonic 3K ROM, select a song, click **Export 7 Channel WAVs...**. Enter minutes and seconds, then select **Export**.

Validation: 10/10 portable tests with MDA_ENABLE_Z80=OFF passed on Linux. Windows GUI and Z80 export not tested locally because Windows isn't available and the Z80 source fetch is network-blocked in this environment.
