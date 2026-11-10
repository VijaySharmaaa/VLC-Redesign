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

## Step 3: spikes (2026-11-10)
Each spike is a throwaway branch. Full reports and evidence are in `D:\Projects\VLC\spike-results\<a-d>\` (not committed).

| Spike | Branch | Result |
|---|---|---|
| a. Frameless window, 40 px title bar | `spike/a-frameless-titlebar` | Works. Remove only the caption with Qt's `WindowsCustomMargins` and hit-test the title bar in `WM_NCHITTEST` (main HWND only). Snap layouts, Aero snap, resize, shadow, Alt+Space, maximize, F11 and dialogs all hold. **Don't use `DwmExtendFrameIntoClientArea`: it suppresses the snap-layouts flyout.** Gaps: minimal view keeps the bar; hiding the bar kills menu shortcuts. |
| b. Overlays above the video | `spike/b-video-overlay` | Works. One host tracks `VideoWidget`, with one frameless translucent `Qt::Tool` window per overlay (badge, bar, toast). The video shows through, positions track every layout change, the overlays aren't topmost and don't take focus, and there's no measurable CPU cost. Gap: drops on the bar must be forwarded. |
| c. Mica / dark title bar | `spike/c-mica-dark-titlebar` | Works with a limit. The runtime-loaded dark title bar follows the system setting. Mica needs the frame extended, which must cover only the title bar and sidebar: over the video area, the letterbox turns see-through. The forced Windows 10 and no-dwmapi fallbacks paint solid `--chrome`. Tie the palette and the title bar to one theme setting. |
| d. Spectrum visualizer | `spike/d-visualizer` | Works. A pass-through `qt_spectrum` audio filter submodule in the Qt plugin feeds time-stamped band levels to a bars widget. Tones light the right bars, silence and pause are flat, and it costs about 1.5% of one core. The `visual` module can't be styled. |

**Conflict between a and c:** spike c's Mica needs `DwmExtendFrameIntoClientArea`, which spike a found suppresses the snap-layouts flyout. Phase 2 must test whether extending only a 40 px top strip keeps the flyout; if not, use solid `--chrome` instead of Mica.

### Harness fix
- **Root cause:** setting `APPDATA` doesn't redirect VLC's config dir on Windows (`SHGetFolderPath`, `src/win32/dirs.c:282`), so test runs wrote the developer's real `vlc-qt-interface.ini`.
- **Fix:** the harness now uses VLC's portable mode (`dirs.c:260-275`) with a fresh `portable\` folder per session, and refuses any vlc.exe outside the project.
- **Verified:** a full baseline left the real `%APPDATA%\vlc` byte-for-byte unchanged.

## Phase 1: review fixes (2026-11-10)
Evidence and scripts: `D:\Projects\VLC\spike-results\p1-fix\` (not committed).

### Manual test steps
- Simple Preferences > Interface: open the Language, Theme and "Force window style" combos with the mouse in light and dark. Each list opens next to the combo, at item height, with every item visible and no scroller (the Language list is long and scrolls).
- Start with `MainWindow/QtStyle=NoSuchStyle` or `=System's default` in `vlc-qt-interface.ini`: the main window keeps the themed look. Save Simple Preferences: the key becomes `QtStyle=`.
- Open Media, Tab to "Show more options": an accent ring (>= 3:1) surrounds the label. The Play split button shows a themed arrow, not a black block.
- Windows high contrast (cannot be automated here): start VLC with a high-contrast theme on. The time tooltip, seek and volume sliders and round buttons use the system colours (text, window, highlight), not orange or grey tokens.
- Plugins and extensions with add-ons installed: rows of different types show different corner tints in both themes; all category labels and "Find more addons online" are fully visible.
