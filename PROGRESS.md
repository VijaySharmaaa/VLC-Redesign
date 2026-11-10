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
| Old config with `qt-dark-palette=1` | **fixed** | The combo showed "Follow the system" while the app was dark; now shows Dark and migrates on save (ce4cd5a34f; `p1-edge\t5c`, `t5d` runs). |
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

## Phase 2: review and audit fixes (2026-11-10)
Evidence: `spike-results\p2-fix\` (`before\`, `after\` from `probe.py`/`probe2.py`; `edge\fix_edge.py` + `fix_edge.log` for audit items A-G; `baseline-dark\`, `baseline-light\`, `names.txt`: 9 unnamed).

### Manual test steps
- At 125 % scaling: menu titles, sidebar labels and the page title keep their relative sizes (13/13/26 px × 1.25); caption buttons are 58 px wide.
- Start VLC: Home is open and active, no focus ring; Space plays/pauses; Tab reaches "Open media".
- Play a video, click Home, click Home again (or the cone): the video is back. Ctrl+L, Ctrl+L: the video is back.
- F11: menus stay, window buttons hide; `f` then Esc while F11 is on: menus come back.
- No access-key underlines until Alt is pressed; then menus and menu bar show them.
- Minimal view (Ctrl+H), Alt+Tab away and back: the menus don't toggle (not reproduced automatically: Windows delivered no Alt release to VLC in either build).
- Light theme: active sidebar label #9a4800, 4.8:1.
- Explorer drag over the library, the video and the title bar: the drop overlay appears, the drop plays (only the window centre was automated).

### Edge cases (rows added by the audit)
| Case | Result | Notes |
|---|---|---|
| Quarter snap on a 1080p work area | **fixed** | client 1000×510 (`edge\d-quarter.png`). |
| Maximised stock ini | **fixed** | zoomed, client = work area (`edge\c-old-ini-maximised-screen.png`). |
| Leaving video fullscreen by end of media | **fixed** | back at y 150 like vlc-pkg-d (was 188). |
| Docked playlist toggled across the rail breakpoint / minimal view | **fixed** | widths stable over 10 toggles; minimal keeps its height. From 1600 px the window returns at 1400 px with the same stack width (both widths fit the stored stack size; the smaller is taken). |
| `TaskbarButtonCreated` twice | **fixed** | list and connections once, buttons re-added per message, no ThumbBar errors. |
| Toast over the subtitle it announces | **fixed** | `edge\d03-srt-toast.png`. |

### Edge cases (`redesign/docs/EDGE_CASES.md`), Phase 2
Evidence: `D:\Projects\VLC\spike-results\p2-edge\` (audit on the merged build, `edge.log`), `...\p2-fix\` (after the fixes: `after\`, `edge\fix_edge.log`), lane evidence `...\p2a\`, `p2b\`, `p2c\`. Not committed. Machine: one 1920×1080 monitor at 125 %, taskbar at the bottom.

| Row | Result | Evidence or reason |
|---|---|---|
| Minimum size | **fixed** | 800 px wide; height is the 480 token or half the work area, whichever is smaller, so quarter snaps fit (deviation from "800 × 600", see below). Clamped to the screen's work area. |
| Maximised; restore after maximise; Win+↑/↓; title-bar double-click | **pass** | The maximised client ends at the work-area bottom, so the taskbar stays visible (`p2-edge\w01b`). Restore returns the exact rectangle. |
| Snapped half / quarter; snap layouts | **pass** / **fixed** | Half 958×1019. Quarter was clamped to 600 px high; now 510 (fix commit 309ff877a4). The snap-layouts flyout appears over the maximise button (`p2a\02-snap-flyout.png`). |
| Alt+Space | **pass** | The system menu opens; Esc closes it. |
| DPI change, monitor drag, multi-monitor fullscreen | **deferred** | One monitor only. Metrics now scale with the logical DPI through `Theme::px()` (3e6992cd95). 150/200 % untested. |
| Windows 10 | **deferred** | No Windows 10 machine. The native caption fallback is implemented but not run. |
| Menu bar hidden/shown, minimal view (Ctrl+H ×5), `--qt-minimal-view` | **pass** | The window rectangle is identical each cycle, idle and with video. Alt reveals the menus; after Alt+Tab the flag clears (fix). |
| Fullscreen with and without the FSC; F11 | **pass** / **fixed** | With the FSC off, keys still work; menu shortcuts fire once in every view. F11 hid the menus after the merge; now it keeps them (309ff877a4). The window came back 38 px lower after end-of-media fullscreen; fixed. |
| Docked vs detached playlist | **pass** / **fixed** | Ctrl+L opens and hides the separate window when undocked. Docked toggles made the window shrink (1300 → 1012 px over 20 toggles) and lose 28 px in minimal view; both fixed, widths stable over 10 toggles. |
| Old `vlc-qt-interface.ini` (normal, off-screen, maximised) | **pass** / **fixed** | Normal and off-screen restore on screen. A maximised stock ini restored as a fake maximise; now truly maximised. |
| Systray on/off, start minimised, pause on minimise | **pass** | Tray icon follows the option; nothing shows when starting minimised; minimising paused playback. |
| Taskbar thumbnail buttons | **pass** / **fixed** | Present; the task-bar list was created again on each `TaskbarButtonCreated` (a leak); now created once. |
| `-I skins2` | **pass** | Skin windows open and play. |
| Keyboard: tab order, visible focus, Esc order, typing in a field | **pass** (partial tab order) | Focus is visible; typing "f m" in a field triggers nothing; Esc closes the menu before leaving fullscreen. Tab cycles only Open media → Home → Preferences: the deck controls were never focusable in stock VLC; Phase 3 makes them focusable. |
| Screen reader names | **pass** (no regression) | 9 unnamed controls, as at baseline; all new title-bar and sidebar buttons are named. |
| Rapid toggling 20× (Ctrl+H, Ctrl+L, F11, pages) | **pass** after the fix | GDI/USER handles stable; the Ctrl+L width drift is fixed (above). |
| Drag and drop (files over library and video, `.srt`, URL) | **pass** | The overlay hides on leave and drop; the `.srt` drop shows its toast, now lifted above the subtitle line (fix); a URL drop enqueues. Drops onto the title bar weren't tested. Shift = enqueue isn't implemented (decision D2). |
| Closing during playback, with a toast, with the drop overlay | **pass** | Exit code 0 within 0.3 s each. |
| Animations off | **pass** | The toast and drop target don't animate when Windows animations are off, but still show and hold 2.4 s (lane C test; the setting was restored after the test). |

**Deviations recorded:** minimum height 480 px instead of 600, so Windows quarter snaps fit on 1080p screens; light-theme accent text `#9a4800` instead of `#B35300` (4.79:1 on the active fill); no "hold Shift to add" hint on the drop target (D2).
**Not Phase 2:** Ctrl+C (Open Capture Device) freezes the UI for over 90 s while probing devices, also in the stock build (`vlc-pkg-d`), so this predates the redesign.

## Phase 3: review fixes (2026-11-10)

Evidence: `D:\Projects\VLC\spike-results\p3-fix\` (scripts `p3fix.py`, `audit_queue.py`, logs `dark-log.txt`, `queue-log.txt`).

### Manual test steps
1. Speed: open the speed popover, drag its slider or press Slower/Faster: the deck label shows the exact rate (e.g. 0.70×), no preset is checked; Reset (1×) returns to 1.00× and checks 1.00×.
2. Speed popover open, then Tools > Customize Interface > Close: the popover closes with no warning in `-vv` (its button was deleted).
3. View > Minimal Interface is unchecked on a library page with the now-playing bar, checked after Ctrl+H, also in the right-click popup of the video.
4. Advanced Controls off, Ctrl+H, Shift+L twice: A–B shows accent after leaving minimal view; Shift+L once more clears it.
5. Mini player up, `qt-auto-raise` = video, add a video: the mini player comes forward, the main window stays hidden. F in the mini player: the mini player goes, the video is full screen; Esc; View > Mini Player is unchecked.
6. Immersive layout, video playing: Tab from the window puts the keys on the bar's first control (visible focus); Tab moves along the screen order; Space toggles play; Esc gives the keys back. Hovering or clicking the bar never takes them.
7. Ctrl+L / Ctrl+E: the deck's playlist / effects button shows open (accent glyph, fill) while the panel / dialog is up, also after closing them with their own close button.
8. Width 860–1280 px with Advanced Controls on: nothing clips; stop, loop, random, speed, mini player and full screen drop out (they stay in the menus) and come back when the window widens or the expert row goes.
9. Aspect badge: pick 16:9 from its menu, then press A (hotkey): the badge follows.
10. Focus the now-playing bar's mute or audio mode's Library pill with Tab and press Space: play/pause toggles, the button does nothing; Enter activates it.
11. Toast: drop a short-named .srt: the full name shows (no ellipsis) at 100 % and 125 % scaling.

### Deviations
- The deck's centre status label of the prototype is not built (not in DESIGN_SPEC).
- Segoe UI has no 500 weight: playlist row titles ask for QFont::Medium and Windows renders its nearest face.
- `qt_spectrum` shows in Preferences > Audio > Filters: VLC 3 has no flag to hide a module there (comment in `qt.cpp`).
- While the immersive bar has the keys, Qt shortcuts of the main window's menus (Ctrl+L...) wait until Esc; VLC hotkeys still work.
- Mini player restore button over a cyan frame looks teal: it is the black 0.55 scrim token over the picture, as designed.

### Audit rows without a code change
- F (immersive bar muted at 100 %): Windows keeps the app's audio session mute across runs (mmdevice reports it at start). A session muted with M starts the next fresh-profile session muted in every volume widget; M again clears it (`p3-fix/queue-log.txt`, `F-*.png`). The widgets follow the real state.
- H (handle growth): 100 playlist-panel toggles: GDI 60 → 69 on the first (the panel is built), flat to 100, back to 59. Mini player: 60 → 68 on the first cycle (the window is built and kept), flat to 10; longer runs lost the window to the automation, so cycles 11-100 are a manual step (open/close the mini player 100 times, Task Manager GDI objects stay flat).
