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

### Edge cases (`redesign/docs/EDGE_CASES.md`), Phase 1
Evidence is in `D:\Projects\VLC\spike-results\p1-edge\` (audit), `...\p1-fix\` (after the fixes) and `...\p1-gate\` (readiness review). None of it is committed.

| Row | Result | Evidence or reason |
|---|---|---|
| Windows 10 vs 11; no crash when DWM APIs are missing | **pass** on 11 / **deferred** on 10 | Every DWM call returned S_OK on build 26300 (`p1-edge` t1–t7 logs). `win_theme.hpp` loads `DwmSetWindowAttribute` at runtime and returns `E_NOTIMPL` below build 17763 (dark) / 22000 (caption colour). The Windows 10 path was only read, not run: no Windows 10 machine. |
| Light/dark switch while running | **pass** | `WM_SETTINGCHANGE "ImmersiveColorSet"` → one reload per message; switching Follow → Dark → Light in Preferences flips every title bar and the palette (`p1-gate` 11-13, `gate2.out`). A theme forced on the command line wins over the saved value, as all VLC options do. |
| High-contrast mode | **fixed**, then **deferred** (live check) | Painted widgets ignored the system colours (code review); fixed in 499999c7eb. Not run: turning high contrast on changes the Windows theme. Manual step listed above. |
| Animations off | **n/a** | `Theme::duration()` returns 0 when they're off, but nothing animates yet (Phase 2+). |
| `-I skins2`; skins2 not affected by the theme | **pass** | The skin is unchanged; Qt dialogs opened from it are themed (`p1-edge\t7-montage.png`). |
| Theme switch while running in skins2 mode | **deferred** (known limit) | No Qt main window receives `WM_SETTINGCHANGE`; marked `shortcut:` in code (488556f87a). Restarting applies it. |
| `--qt-minimal-view` | **pass** | `p1-edge\t6c-main.png` |
| First run (privacy dialog) | **pass** | Themed with a dark title bar; Continue works (`t6a-privacy.png`). |
| Clean profile vs old `vlc-qt-interface.ini` | **pass** | Old ini with custom toolbars, `QtStyle=Windows` and missing recents restores correctly (`t5a-main.png`). |
| Old config with `qt-dark-palette=1` | **fixed** | The combo showed "Follow the system" while the app was dark; now shows Dark and migrates on save (ce4cd5a34f; `p1-int\t4.py` run). |
| "Force window style: System's default" saved / unknown style | **fixed** | Fell back to the Vista style; now Fusion (a89919df99, `p1-fix\s2-compare.png`). |
| Every combo drop-down opens at a normal size | **fixed** | Popups were screen-high and blank (`p1-edge\t2c-popup.png`); fixed in f3fbe93595 + c661414531 (`p1-fix\s1-after-montage.png`). |
| Translations (German, RTL, CJK) | **deferred** | The package is built without NLS (`build.sh` without `-i` passes `--disable-nls`), so no `.mo` files exist to test with. To check with an installer build in Phase 6. |
| Keyboard: visible focus, Esc closes the top dialog | **fixed** / **pass** | Check box/radio focus was 1.84:1 and text-field rings 2.4:1 in light; now ≥ 4.6:1 (ab0ec2d740, b21f1d8f4a). Esc closes Preferences and Effects. |
| Screen reader names | **pass** (no regression) | `harness.py names`: 9 unnamed, same as the stock baseline (`p1-gate\names.log`). Naming them all is Phase 6. |
| Rapid open/close of Preferences/Effects (20×) | **pass** | GDI/USER handles stayed at 74/36; no crash. |
| Closing VLC with a dialog open | **pass** | Exit code 0 in 0.2 s. |
| Hotkeys | **pass** | Space, F, Esc, Ctrl+P, Ctrl+E (`p1-gate\gate3.out`). |
| Preferences Save/Cancel/Reset writing the same variables | **pass** (theme only) | The Theme combo writes `qt-redesign-theme` and clears `qt-dark-palette`; other panels unchanged in this phase. |

**Known for later:** the Help dialog's links are blue in the dark theme (Phase 6); the Effects window now uses the normal dialog frame (it gains minimise/maximise buttons) so its title bar follows the theme (7b6701878a).
