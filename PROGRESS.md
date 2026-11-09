# Progress

## Phase 0: build environment and baseline (2026-11-10)

### Done
- **Full build:** unmodified 3.0.x (3.0.25-dev, `c128d1f15c` plus the redesign kit) built for Windows x64 with the VideoLAN image `vlc-debian-win64-3.0:20260930101942` and prebuilt contribs (`7db859b5d7…`). It took 8 min 14 s and exited 0. Package: `D:\Projects\VLC\vlc-pkg`.
- **Fast loop:** `redesign/tools/qt-rebuild.sh` rebuilds only `libqt_plugin.dll`, strips it and copies it into the package in about 16 s per changed `.cpp`. Verified: after a rebuild, the harness still captures 7 of 7 states.
- **Test harness:**
  - `redesign/tools/harness.py` puts vlc.exe into states through the rc interface, sends keys through pywinauto (UIA) and takes screenshots with `redesign/tools/shot.ps1`.
  - It refuses to send keys unless the test VLC is the foreground window, and fails loudly if an expected window or state never appears.
  - 3 runs in a row captured 7 of 7 states each time.

### Baseline screenshots (stock 3.0.x UI)
`redesign/.local/baseline/` (not committed):
1. `01-main-idle`: main window, nothing loaded
2. `02-main-video-paused`: video paused at 0:10
3. `03-playlist`: playlist view (Ctrl+L)
4. `04-effects`: Adjustments and Effects (Ctrl+E)
5. `05-open`: Open Media (Ctrl+Shift+O)
6. `06-prefs`: Simple Preferences (Ctrl+P)
7. `07-main-audio`: audio-only file playing

### Findings
- **Accessibility baseline:** in the stock main window, 9 controls have no accessible name: both sliders (seek, volume) and 7 control-bar buttons. The menu items and title-bar buttons have names. The redesign must name every one (`harness.py names` checks this).
- **rc interface (`modules/control/oldrc.c`):** it refuses playlist and input commands while paused but still prints `returned 0 (no error)`. Scripts must resume with `pause` first.

### Manual test steps
- Run `py redesign/tools/harness.py baseline --vlc D:\Projects\VLC\vlc-pkg\vlc.exe`: 7 PNGs are written and no error is printed.
- Run `py redesign/tools/harness.py names --vlc D:\Projects\VLC\vlc-pkg\vlc.exe`: every control is listed, with a count of unnamed ones (9 at baseline).
