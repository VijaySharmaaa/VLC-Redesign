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

### Edge cases (`redesign/docs/EDGE_CASES.md`), Phase 3
Evidence: `D:\Projects\VLC\spike-results\p3-edge\` (audit on the build before the fixes, `log.txt`), `...\p3-fix\` (after the fixes: `dark-*`, `light-*`, `queue-log.txt`, baselines), lane evidence `p3-1\` to `p3-4\`. Not committed.

| Row | Result | Evidence or reason |
|---|---|---|
| Nothing loaded (idle) | **pass** | Seek and tracks disabled; labels `--:--`; no division by zero (the seek wheel guard was added in lane 1). |
| Live streams, unknown length | **fixed** | The seek stayed enabled because `can-seek` was read before the demuxer set it; it's now re-read, and also cleared on stop. "Live" label; no negative time (`p3-fix` A). |
| Very short and very long media; chapters | **pass** | A 0.6 s clip ends cleanly; 10 h shows `10:00:00`; chapter gaps and the chapter tooltip appear. |
| Audio-only, video-only, no art, huge art, transparent art | **pass** | All display; audio-only opens audio mode with the spectrum. |
| No tracks, 20+ tracks, tracks added mid-playback | **pass** | "No tracks"; 3 audio + 20 subtitle tracks scroll; a dropped `.srt` shows a toast (full text after fix E) and appears as a selected track. |
| Speed extremes | **pass** / **fixed** | 0.25×, 4.00× and 1.30× labels, no preset ticked. The fine speed slider had been removed by the new popover; restored under the presets. |
| Volume 0/100/125, `qt-max-volume` 200/300, mute → unmute | **pass** / **fixed** | Tick and boost scale with the maximum; unmute restores the level. The boost colour now covers only the part above 100 %. A "muted at start" report was Windows remembering the app's mute between runs, not a widget bug. |
| Buffering that never finishes; unreachable URL | **pass** | The ring or loading sweep shows; the error dialog appears; quitting during buffering exits cleanly. |
| Aspect ratios, rotated video, Unicode/emoji/long names | **pass** | 4:3, 21:9, 1:1 and rotated display correctly; no crash on long or emoji names. |
| Saved toolbar strings from stock 3.0; empty and corrupt strings; editor round trip | **pass** / **fixed** | They load unchanged. The stock disc buttons were unnamed; they're named now (0 unnamed). The editor lists and saves the two new buttons. |
| Immersive layout; FSC on/off in video fullscreen | **pass** / **fixed** | Only one bar shows in fullscreen. The immersive bar wasn't reachable by keyboard; Tab now moves focus into it, and Esc or idle gives it back. |
| Docked vs detached playlist; XS drawer | **pass** / **fixed** | The detached window still works. The drawer wasn't reachable while playing: the deck's minimum width kept the window at 1125 px. Optional deck items now drop when space runs out (`p3-fix` 8/C). |
| Mini player | **fixed** | Auto-raise re-showed the hidden main window, and F went fullscreen behind the mini player. Both now respect it; the View menu check stays correct. |
| Minimal view and fullscreen with the new deck | **pass** / **fixed** | The "Minimal Interface" check was wrong while the now-playing bar showed; fixed. |
| Keyboard only: tab order, Space, Esc, arrows in popovers | **fixed** / **pass** | Space activated whichever button had focus (mute, Library); one filter now sends Space to play/pause from every button. Tab order now follows the screen. Esc closes a popover before leaving fullscreen. Shift+L (A–B) works with the deck hidden. Main-window Tab order is a manual step: the video window keeps keyboard focus during automation. |
| Screen reader names | **fixed** | 0 unnamed controls (was 9 at baseline). |
| Rapid clicks and open/close 30–100× | **pass** | Play/pause spam ends playing. The panel and mini player take about 9 GDI handles once when first built, then stay flat (panel over 100 toggles; mini player over 10 automated cycles, the rest is a manual step). |
| Closing with a popover, the mini player or buffering | **pass** | Exit code 0 each time. |
| `qt_spectrum` never saved | **pass** | After saving Effects and Preferences, `audio-filter=equalizer` is saved with no `qt_spectrum`. It still appears in the advanced Preferences filter list (VLC 3 can't hide a submodule there; documented in `qt.cpp`). |
| 10,000-item playlist | **deferred** (pre-existing) | Loading freezes the interface for about 45 s; stock VLC freezes for about 40 s on the same load. The cause is in the shared playlist model's per-item updates, which predate the redesign. Scrolling and filtering the loaded list are fast. |

**Decisions recorded (owner to confirm):** the DVD menu buttons leave the default deck; remaining time is shown by default; resume and the tracks popover's details come from stream info; the paused badge resumes on click; the docked playlist is a right-side panel; no status label in the deck centre (it's in the prototype, not the spec).

## Phase 4: review fixes (2026-11-10)

Evidence: `D:\Projects\VLC\spike-results\p4-fix\` (scripts in `scripts\`: `t1_quit.py`, `t_home.py`, `t_empty.py`, `t5_ini.py`, `t2_selector.py`, `t_pages.py`; screenshots and `*.txt` logs). Not committed.

### Manual test steps
1. Quit with an offline share in the recents: a recent `\192.0.2.1\share\a.mp4` (or any unreachable server), open Home, quit at once: VLC exits in under a second (was 19 s; `t1_quit.py`).
2. Continue watching: play a file of 60 s or more to its middle, stop, click its card on Home: it resumes at the saved time without the resume question. Stop, then Play (or double-click it in the playlist): it starts at 0, or asks as `qt-continue` says.
3. Network page > Podcasts first, then (playlist undocked) Ctrl+L and pick Podcasts in the playlist window's tree: the subscriptions are listed.
4. Network page with a UPnP/DLNA server: open the server, switch the server off while its list is shown: the page goes back to the tiles, no crash. With the keyboard on the second server tile, a server appearing or leaving keeps the focus on the tile at that index.
5. Use the same profile with stock VLC 3, play something there, then start this build: Home shows "—" for lengths and dates instead of another file's (they come back as files are played again).
6. First run: Home, Videos, Music and Network show no deck and no now-playing bar; play anything and the bar shows on library pages. Ctrl+H, the hotkeys and Tools > Customize Interface still work there.
7. Empty Home at 1018x519 and 1280x800: one dashed box (the library pages' empty state), no scroll bar, both buttons visible; hover them: "Add a folder to the playlist and play it (Ctrl+F)", "Open media (Ctrl+Shift+O)". With `--no-qt-recentplay` the note about the setting shows.
8. Home search (top right): typing filters the cards and the table by name or path; no match shows "No results".
9. Home at 1920/1280/1024/800 px with a recent played today on a 12-hour clock: the When column shows the whole "Today, 10:52 AM".
10. Network page: Streams reads Open network stream, Icecast directory, Podcasts, and Tab follows that order.
11. Videos grid at 1280 px: the right edge of the last card column lines up with the search field's right edge.
12. Tab to a primary button (Add a folder): its border shows in both themes.
13. Videos: open folder A, back, folder B, back, A again before A's listing came in: A's items are listed once.

14. At 1600 px, Home, Videos, Home, Music, Network, Home: the window stays 1600 px and the sidebar stays full (`t_drift.py`; was 1400 px with a rail).
15. Network > Icecast directory: the header says "Loading..." with an empty list until the stations come; an empty list says "No results" after about 10 s.

### Deferred
- Quit while the Icecast directory loads takes up to the download time (10-17 s in 4 of 5 runs, `ice-after.txt`, `ice-quit.log`): `modules/lua/services_discovery.c:287` runs the script's `main()` with cancellation off and `Close()` joins it (`:264-265`), so the core waits for the whole download and parse. Stock VLC does the same from its selector. Outside `modules/gui/qt`; the Qt interface has already finished at that point.

### Deviations
- Primary button focus uses `@tx@`, like the accent-filled play button, not `@focusRing@`: in the dark theme `@focusRing@` is the accent, the button's own fill, so the ring would not show.
- The network tile glyph's `+ 1` became exact centring arithmetic (`(TileIcon - TileGlyph) / 2`), not a token: it only made up for `QRect::center()` rounding.
- Home's search scrolls with the page (Home is one scroll area); on the other pages the header stays put.

### Edge cases (`redesign/docs/EDGE_CASES.md`), Phase 4
Evidence: `D:\Projects\VLC\spike-results\p4-edge\` (audit on `dc0c90c058`, before the fixes), `...\p4-fix\` (after the fixes), lane evidence `p4-1\`, `p4-2\`. Not committed.

| Row | Result | Evidence or reason |
|---|---|---|
| Empty library (no folder, missing folder, permission denied) | **pass** / **fixed** | "Scanning…" then the empty state; both buttons work. Home's empty state was cut off in short windows and differed from the other pages; now one shared component that fits (`p4-fix` 8/9). |
| A folder with 10,000+ files | **pass** | Worst UI stall 188 ms over 60 s; scrolling ≤ 211 ms; search 3 ms; quitting mid-scan exits cleanly. |
| Recents pointing at deleted files or offline shares | **pass** / **fixed** | Deleted files dimmed with "not found". An offline share no longer blocks quitting (18.8 s → 0.2 s, one never-deleted worker pool). |
| Unicode, emoji, very long paths | **pass** | Shown; a 343-character path elided in the middle; no crash. |
| Old config without recents lengths/dates; stock VLC sharing the ini | **pass** / **fixed** | "—" shown. If stock VLC reorders the list, the extra lists are ignored instead of being misaligned (`listForExtras`). |
| More than 30 recents, removal, `qt-recentplay` off, `qt-recentplay-filter` | **pass** | All four lists stay aligned; the off note shows; filtered entries hidden. |
| Resume from Continue watching; replay from the playlist; `qt-continue` 0/1/2 | **pass** / **fixed** | The card resumes. Replay used to jump back to the old resume time (`:start-time` left on the item); now a one-shot resume, replay starts at 0. |
| Folder browse/back, search, album → audio mode | **pass** | Search never changes the docked playlist; the album is appended and audio mode opens. |
| Window width on page switch | **fixed** | 1600 → 1400 px drift on Home → Videos → Home; now stable over 12 switches. |
| Network: discovery only on first visit; stream tiles; loading state | **pass** / **fixed** | UPnP/mDNS/SAP load on the first visit only. The Open network stream tile works. Lists now show "Loading…" until items arrive. |
| Windows Firewall prompt on the Network page | **known** | Loading UPnP makes Windows ask whether VLC may use the network, as stock VLC does when its UPnP entry is opened. Test runs pressed Cancel (never Allow); the test package may now have a block rule. |
| Quitting while the Icecast directory downloads | **deferred** (core) | 10–17 s quit in 4 of 5 runs: `modules/lua/services_discovery.c:287` runs the script with cancellation off and `Close()` waits for it (`:264-265`). Stock VLC behaves the same; the fix is outside `modules/gui/qt`. |
| UPnP servers appearing and disappearing; removable drives and discs | **deferred** | No server or hardware here. The crash risk when a shown server leaves (proxy reset mid-removal) is fixed by a queued connection, and focus restore on tile changes is fixed; both have manual steps. |
| Files added while the page is open | **stock limit** | VLC 3's media-folder discovery doesn't watch for changes. |
| Old playlist window's selector beside the library | **pass** / **fixed** | The library doesn't re-root the docked playlist. The selector's Podcasts entry was empty after the Network page loaded podcasts; fixed. |
| Rapid page switches and folder open/close (30–60×) | **pass** | GDI/USER handles stable; worst stall 26 ms. |

**Decisions recorded (owner to confirm):** P4-1 recents keep lengths and dates; P4-2 "Add a folder" adds the folder to the playlist; P4-3 album cards append and play; P4-4 Continue watching always resumes; P4-5 no Windows-shares tile (libdsm not in this build); P4-6 network discovery starts on the first visit to Network. Not added: a "Folders" sidebar section (the design shows custom folders, which VLC 3's media folders can't hold).

## Phase 5: review fixes, Preferences page and switch (2026-11-10)

Evidence: `D:\Projects\VLC\spike-results\p5-fixA\` (`verify.py`, `shots.py`, screenshots, `verify-1.json`, `verify-2.json`). Not committed.

### Manual test steps
1. Ctrl+P at 1600 px, dark and light: every section is cards of rows (name, one-line description from the option's long text, control on the right). Group boxes are cards with their titles; nothing of the old forms is missing except the interface preview images and separator lines.
2. Interface: "Look and feel" row with "Use native style" / "Use custom skin"; pick custom skin: the skin file row shows with its Browse button and the skins site link under it.
3. Switches: click one, the knob slides (150 ms; at once with Windows animations off) and "On"/"Off" beside it follows. Tab to one: an accent ring around the pill. Space toggles it. Untick "Integrate video in interface": "Resize interface to video size" greys out, name included.
4. Audio: tick "Always reset audio start level to": the slider is the accent slider (track and round knob).
5. Audio: change Output module: the Device and passthrough rows show and hide with the module.
6. Change one switch in each section, Save: `vlcrc` gets the same variable names as before (`qt-pause-minimized`, `audio-time-stretch`, `video-deco`, `video-title-show`, `input-fast-seek`, `qt-disable-volume-keys`).
7. Change a switch, press Esc on the page: it is discarded, as with Cancel, and the view Preferences was opened from comes back (see "Preferences: one sidebar").
8. View the Interface section, play a video full screen, Ctrl+P: the Preferences dialog opens; change "Pause playback when minimized", Save. Leave full screen, Ctrl+P: the page shows the new value; change an Audio option and Save on the page: `vlcrc` keeps the dialog's value.
8b. The same with the dialog's Reset: the page shows the reset values (Window decorations back on) and a page Save keeps them. Any other writer of `vlcrc` (Effects > Save) takes the same path.
8c. Mini player (deck button), Ctrl+P: the Preferences dialog opens (the main window is hidden). Other menu shortcuts (Ctrl+O, Ctrl+E...) also work from the mini player now.
9. Window under the M breakpoint, then wide again: the section list shows its labels one under the other (it overlapped before).

### Deferred
- Preferences > Audio, volume slider while "Always reset audio start level" is off (slider disabled): the knob is still an 18 px square in the toggleOff colour. Restating the groove and handle rules for `:disabled` (with and without `:horizontal`) changed only its colour, not its size or radius: Qt 5.6's stylesheet style seems to fall back to the base style's handle rect there. Needs Qt's `qstylesheetstyle.cpp` to trace; the enabled slider is themed.
- Effects > Save after ticking "Enable equalizer" did not rewrite `vlcrc` in this lane's build (time unchanged, `audio-filter` default), so the page rebuild after an Effects save could not be shown end to end. The watcher reacts to any rewrite of `vlcrc` (shown with the dialog's Save and Reset).

### Deviations
- Built panels are rebuilt whenever `vlcrc` is saved by anything else (watched folder, file time compared): edits pending on the page are dropped then (marked `shortcut:` in `prefs_page.cpp`).
- P5-5: edits on the page apply on Save (or are dropped by Cancel/Esc), not immediately as the reference's "Changes apply immediately" says: the panels keep the dialog's apply-all model, so a half-typed path or proxy is never written.
- The switch's knob is OnAccent (on) and Text2 (off), as in the prototype, not white.
- Group box titles stay above their cards (the design has one untitled card per section; VLC's forms have several groups).
- Rows keep the forms' order; two-column check boxes became one row each.
- Hotkeys keep the action table (search, list, double-click to assign) as one block; the wheel and volume-key options are rows. No keycap control.
- Selects and fields alone in a row are at least 240 px at every width (the prototype narrows them to 180 at XS).

### Edge cases (`redesign/docs/EDGE_CASES.md`), Phase 5
Evidence: `D:\Projects\VLC\spike-results\p5-edge\` (audit on the merged build before the review fixes), `...\p5-fixA\` and `...\p5-fixB\` (after the fixes), lane evidence `p5-1\` to `p5-3\`, `p5-int\fix-1\`, `fix-2\` (baselines after the fixes). Not committed.

| Row | Result | Evidence or reason |
|---|---|---|
| Preferences: Save, Cancel, Reset (with confirmation) | **pass** | Same variables as stock 3.0 except the planned `qt-dark-palette` → `qt-redesign-theme`. The registry key `HKCU\Software\VideoLAN` was absent before and after; `cleanLang` only opens it. |
| Simple ↔ all settings; the tree dialog's Save | **pass** | Show all settings opens the complete tree; its Save rewrites `vlcrc`. |
| Settings saved elsewhere while the page holds old values | **fixed** | The page used to write stale values back after a save in the fallback dialog, a Reset or the Effects sheet; it now rebuilds when `vlcrc` changes behind it (pending edits are then dropped, marked `shortcut:`). |
| Old stock `vlcrc` (`--config`), portable mode | **pass** | Values show and survive Save; only the portable folder is written; `%APPDATA%\vlc` untouched. |
| Pending edits across sections and page switches; Esc | **pass** | Kept until Save or Cancel (decision P5-5, confirmed by the owner: changes apply on Save). Back/Esc discard and return (owner decision 2026-11-10, "Preferences: one sidebar"). |
| `qt-advanced-pref`; Ctrl+P from fullscreen, minimal view, mini player | **pass** / **fixed** | The tree opens directly with `qt-advanced-pref`. Ctrl+P did nothing from the mini player; menu shortcuts now work there. |
| Reset with the simple panels never built (advanced mode) | **fixed** | `cleanLang()` went through a possibly NULL panel; now static. |
| Effects Save semantics vs stock | **pass** | Save only on the Audio/Video tabs; `equalizer-preset` and `audio-filter=equalizer` saved; sync never saved. |
| Equalizer chips, bands, "Custom", preamp | **fixed** | "Custom" never showed when a band moved (stock bug); fixed. The preamp row was missing from the redesigned tab; restored. |
| Every effects sub-panel reachable | **pass** | "More" lists all nine other panels; v4l2 stays a tab when present. |
| Effects sheet across breakpoints (dock ↔ XS drawer) | **fixed** | After being the XS drawer the sheet came back blank (and so did the docked playlist): it kept its frameless flag when docking back; fixed in both. |
| Sheet with fullscreen, library trips, playlist exclusivity | **pass** / **fixed** | Esc now closes the sheet before leaving fullscreen. The separate playlist window no longer closes the sheet; only the docked panel does. |
| Enter in the Effects sheet | **fixed** | Enter on a focused button closed the sheet; it now presses the button. |
| Open media: File / Disc / Network, recent streams, split menu, Ctrl+O native picker, XS | **pass** / **fixed** | Recent streams are a list under the URL field. "Show more options" never persisted (stock bug: saved after the dialog was hidden); fixed. Usable at XS. |
| Open media: Capture tab | **deferred** (pre-existing) | Hangs on this machine in stock 3.0 too (DirectShow device scan). |
| Error dialog: unreachable URL, Show log, Retry, "Hide future errors" | **fixed** | Retry was offered for every core error (snapshot, record…) and could restart playback; now only for an input that failed before playback, replaying that item. Enter retried instead of closing; now closes. The checkbox now follows `qt-error-dialogs` every time the dialog shows (stock bug: it started checked). |
| Media information Save Metadata; Go to time Enter/Esc; About | **pass** | Save Metadata writes the file; Go to time jumps on Enter, closes on Esc. |
| Closing VLC with each sheet, page or dialog open | **pass** | Exit code 0 in 0.2–0.3 s; `OpenDialog`, `Mediainfo`, `gototimedialog` saved. `EPanel/geometry` is only written when the sheet is a separate window (no main window). |
| Open/close each surface 50× | **pass** | No GDI growth after first use; the Preferences page grew USER handles by 4 over 49 cycles. |
| Accessible names | **fixed** | The network URL field and the Media information metadata fields were unnamed; now named. 0 unnamed in the names check. |
| Translations | **deferred** | No NLS in this build. |

**Known for Phase 6:** the disabled Preferences volume slider still draws a square knob; Hotkeys keeps its action table (no keycaps); the light URL focus ring now uses the contrast-safe colour; Alt+D is shared by "Disc" and "Capture Device" (stock, would change a translated string).
**Decisions recorded (owner to confirm unless noted):** P5-1 the Effects sheet docks beside the video (floats at XS); P5-2 sync steps of 50 ms; P5-3 Retry replays the failed item; P5-4 prototype rows with no VLC option dropped; P5-5 Preferences changes apply on Save — **confirmed by the owner (2026-11-10)**.

## Preferences: one sidebar (design update, 2026-11-10)
DESIGN_SPEC 8, SCREEN_INVENTORY 2.5.P1–2.5.P6, PLAN-DETAILED 10. Branch `redesign/prefs-sidebar`.

### What changed
- The sidebar is a stack of two pages: library navigation | preferences navigation (back row 32 px with arrow_back and "Back to library"/"Back to player", the "Preferences" heading 22/600 at −0.01em, the 7 sections as 36 px sidebar items with 20 px icons). The active section has the accent tint, accTx text and a 3 px accent bar on its leading edge (inset 9, radius 2), painted from tokens. Below 1200 px: back arrow and section icons with tooltips, no heading. RTL: the back arrow is mirrored and the bar moves to the right edge.
- The page swaps when Preferences becomes the stack's current widget (every entry path), and the sidebar remembers the view it left. Leaving restores the library nav with the item of the page returned to.
- The Preferences page has no section list, rail or resize code any more; the sidebar drives `showSection` (the page keeps the current section). The section name is the page title (26/600, −0.01em); the panels' own "… Settings" titles and Advanced's duplicate title are hidden.
- Back and Esc: back to the remembered view (library page, player, audio mode), dropping unsaved edits as Cancel does. A video that ended meanwhile goes to Home. Esc is in `handleKeyPress` after the Effects sheet and before minimal view; menus, pop-ups and dialogs take it first.
- Ctrl+P in minimal view opens the Simple Preferences dialog, as in full screen and the mini player.
- Tab: back row → sections (one stop, arrows move and switch) → content → Cancel → Save. Root cause of a split Tab chain found and fixed: each combo box's pop-up list was in the order list, `setTabOrder` refuses widgets of another window, and the chain went on from there, so Cancel/Save landed after the first card (this was already in the Phase 5 build).
- Tokens: `Size::SidebarBack*`, `SidebarTitlePad*`, `SidebarSectionGap`, `SidebarBar*`, `Type::NavTitle`, `Type::TitleTracking` (mirrored in `design/tokens.json` and `design/v3/redesign_tokens.hpp`). Removed: the `prefsList`/`prefsSections` QSS, its variables and `Prefs::List*`/`RailW`/`Item*`/`TitleBelow`.

### Owner decisions (2026-11-10)
Save / Cancel stay; "Changes apply immediately" is not added. Back and Esc discard unsaved edits and return to the previous view. Ctrl+P in minimal view opens the classic dialog.

### Deviations
- No "Changes apply immediately" line (owner decision).
- The content still ends in Cancel / Save, and the now-playing bar shows under Preferences during playback, as before.
- Tab enters the section group on its first item, not the checked one (Qt's exclusive-group behaviour, same as the library nav); arrows then move.
- Ctrl+P in the separate (undocked) playlist window does nothing: that window doesn't carry the main window's shortcuts. Unchanged by this work.

### Evidence (`D:\Projects\VLC\spike-results\prefs-sidebar\`, not committed)
Desktop 1920×1080 at 125 %: "100 %" is the native scale, "150 %" adds `QT_SCALE_FACTOR=1.5`; widths are physical window widths (narrowest window: 1018 px native, 1518 px at 150 %).
- Before/after: `before-P1-dark-1280.png`, `before-P3-dark-1280-audio.png`, `before-P5-dark-1024.png` / `after-P1-dark-1280.png`, `after-P3-dark-1280-audio.png`, `after-P5-dark-1024.png`.
- 2.5.P1/P3/P5/P6: `{dark,light}-{100,150}-{1920,1280,1024,800}-{P1,P3-audio}.png` (32 frames); library nav and Open media hidden, the clicked section checked in every frame.
- 2.5.P2: `P2-*.png`, `behave-player.log`: Tools › Preferences from the player says "Back to player"; playback keeps running through Preferences (time 4 → 8 → 12 → 14 s) and a paused video stays paused; Esc from audio mode returns to it; a video stopped while in Preferences → Esc → Home.
- 2.5.P4: `P4-*.png`, `behave-dropdown.log`: the first Esc closes the Output module drop-down only, the second goes back.
- Back/Esc discard: an edit then Back or Esc leaves vlcrc unchanged and the switch at its old value; Save writes it.
- Fast switching (21 clicks), UIA (back row and 7 radio buttons named, the current one selected; 0 unnamed in Preferences; `harness.py names` 0 unnamed), Tab order, rail tooltips, minimal view → "Simple Preferences" dialog, playlist window: `behave-misc.log`, `tabprobe-final.log`, `P5-tooltip-*.png`.
- RTL and a long section name: a throwaway build (not committed) with a test-only env switch, because VLC passes no Qt arguments on Windows and Qt 5.6 ignores LANG: `rtl-*.png`, `long-label-*.png` (ellipsis, no clipping).

### Manual test steps
1. Library page → Ctrl+P: sidebar shows "Back to library", Preferences and 7 sections; Open media and the library nav are gone. Click Audio: accent bar, title "Audio".
2. Play a video → Tools › Preferences: "Back to player"; Esc: the video, still playing. Repeat while paused: still paused.
3. Open the Output module drop-down, Esc, Esc: first closes the list, second goes back.
4. Change a switch, Back: reopen, the switch is unchanged. Change it, Save: it sticks.
5. Narrow the window below 1200 px: back arrow and icons only, tooltips "Back to library (Esc)" and section names.
6. Ctrl+H, Ctrl+P: the Simple Preferences dialog opens.
7. Library page, Ctrl+P: the current section has the focus. Tab to a switch, Space: it flips. Tab to Save, Space (or Enter): saved. Esc: back, and the next Space is play/pause. Alt+S opens the Tools menu (Save has no mnemonic).
8. Play an audio file, Tab to the deck's Mute, Space: play/pause (the now-playing bar's Mute the same). Tab to the sidebar's Open media, Space: the Open Media dialog.
9. Ctrl+P, then start an audio-only file: Preferences stays. Ctrl+H in Preferences: back to the previous view, in minimal view.
10. Play audio, Ctrl+P ("Back to player"), stop: the label turns to "Back to library" and Esc goes Home. Same from a stopped video or the idle cone.
11. Window under 800 px, Ctrl+P, Ctrl+L (drawer), click a section, Esc: the drawer closes, Preferences stays; Esc again goes back.
12. Focus a section, End / Home: Advanced / Interface. Light theme: the active section's bar is the dark accent text colour.

### Review fixes (2026-11-10)
Accessibility audit, design review and code review of the change above. Evidence: `D:\Projects\VLC\spike-results\prefs-sidebar-fix\` (`repro.py`, `before.log` on the reviewed build, `after.log` on the fixed one; not committed). Note: pywinauto's `send_keys` drops a plain `' '`; Space must be sent as `{SPACE}`.

| Finding | Root cause | Fix |
|---|---|---|
| Space never toggled a control in Preferences | `ButtonKeys` (`main_interface.cpp`, from 276186aeea) sent Space on every focused button of the main window to play/pause | Only buttons inside `AbstractController` (deck, immersive, FSC), `NowPlayingBar` or `MiniPlayer` pass Space on; Enter still clicks everywhere (cca7deaeb8) |
| Alt+S did not save | "&Save" clashed with "Tool&s" in the menu bar, so Alt+S cycled focus | Save uses the existing "Save" string, no mnemonic (d975fbf294) |
| Focus placement, Tab stops | Nothing set focus on entry or Back; the main window was a `StrongFocus` Tab stop; the panels' scroll area was put in the Tab chain | Focus on the checked section on entry, on the window after Back/Esc; window `ClickFocus`; scroll area `NoFocus` (68c06ac75c) |
| Light marker 1.78:1 | Bar painted in `Accent` in both themes | `AccentText` in light (deae4b2eb9) |
| Audio start leaves Preferences | The audio-mode check used `isLibraryPage()`, which includes Preferences | Preferences is skipped (e042faef8e) |
| Minimal view strands the page | Minimal view hides the sidebar holding the sections | `toggleMinimalView(true)` leaves Preferences as Back does (e042faef8e) |
| Back label/target | Label from "was it a library page", target fixed only for an ended video | `MainInterface::backTarget()` (player view with nothing to show → Home) drives both, relabelled on input/vout changes (68c06ac75c) |
| Small | — | Panel-title null check (68c06ac75c); Esc closes the XS drawer first; Home/End in the sections; the update-days box named by its option (bd39a96422); one `setBackLabel()` for text, name and "(Esc)" tooltip (68c06ac75c) |

Results (`after.log`): Space flips a switch (1→0) and presses Back; keyboard-only round trip writes `qt-fs-controller=0` and Esc returns with the focus on the window; Alt+S opens Tools; Tab from Save → Back → sections, Shift+Tab from Save walks the content to the sections with no window stop; audio start keeps Preferences, Esc → Home; Ctrl+H leaves Preferences into minimal view; audio stopped in Preferences relabels to "Back to library" and Esc → Home; drawer closes on the first Esc, Preferences on the second; End/Home → Advanced/Interface; spinner named "Number of days between two update checks"; Phase 3: Space on the deck's Mute and Next and the bar's Mute pauses, on Open media opens the dialog; light bar pixel (154, 72, 0), dark (255, 138, 31).

**Deferred (pre-existing, not Preferences):** when the video view goes away (Ctrl+P from a playing video, or just `stop`), Qt loses keyboard focus for good: for an instant Windows activates the next app's window (seen: VS Code), and the focus widget stays empty though VLC is foreground again (throwaway `focusChanged`/`GetGUIThreadInfo` log, `focusdbg*.py`, `focusdbg*-vlc.log`). So from the player, the section is focused and then loses it; keys still reach the window. Needs a trace of the video widget / vout window activation in Phase 6.

## Phase 6: polish (2026-11-10 to 2026-11-11)
Branch `redesign/phase-6` from `redesign/phase-5` (c36cf50573). Six survey lanes (playback screens, library/settings screens, accessibility, code sweep, translations/RTL, performance) produced the fix list; fix lanes a–h, wave 2, r1–r9 and two alignment passes followed, each reproduced first and re-verified after. Evidence: `D:\Projects\VLC\spike-results\p6-*` (not committed).

### Owner decisions
- P6-1 Player and audio mode hide the sidebar; a header row (back arrow, title, meta) sits over the video, as in the renders.
- P6-2 Light theme: progress, switch and boost fills use the darker accent; control outlines darker (≥ 3:1). Dark fills unchanged.
- P6-3 Our drawn caption buttons are hidden from screen readers; Windows' own set is read.
- P6-4 The 14 new source files are listed in `po/POTFILES.in`.
- P6-5 In the normal layout the header hides with the mouse cursor during video playback.
- P6-6 When the window moves to a monitor with another scale, the theme is re-applied at that scale.
- P6-7 `qt-video-autoresize` is off by default (the window keeps its size when a video starts).
- P6-8 Dark immersive glass 0.88 (no-blur fallback, like light).
- P6-9 The stock on-video text is suppressed only for messages a toast already shows (snapshot, subtitle added).
- P6-10 The mouse wheel changes the volume only over the video, the deck and the Now Playing bar.
- P6-11 Esc leaves fullscreen at once, also after Tab moved the keys into the fullscreen controller.
- Go to time keeps the stock Reset button; Media information stays editable (stock features).

### What changed (by area)
- Focus: only the video widget is a native window (`WA_DontCreateNativeAncestors`, `AA_DontCreateNativeWidgetSiblings`), so Tab and screen readers work after media opens.
- Window: library pages and Preferences never resize the window; docked panels never push it past the work area; the XS minimum is reachable with video (expert row hides first); the resume panel grows the window once and gives back exactly that; Ctrl+H restores the pre-minimal width.
- Player: header over the video (P6-1, P6-5); subtitles lift above the immersive bar and the toast follows; buffering ring for slow streams over the video's black; aspect badge shows the picture's ratio; tracks popover one tick per column.
- Keyboard and screen readers: accessible activation (toggled) for sidebar, sections, tracks button, Open media tiles (tab list, arrows, only the checked tile is a Tab stop); deck buttons describe their shortcut; seek bars read as "x of y"; Effects sliders are real sliders (checkable group box accessible cache); no unnamed controls on any surface.
- Theme: per-theme AccentFill / ControlLine; destructive and dim text contrast; tab focus ring; round opaque knobs; text centred on the capital height (`Theme::textNudge`); rail items keep a fixed square; menu shortcuts on the label's baseline; per-monitor scale with pixel fonts and rescaled pages; focus-ring width from a token.
- Dialogs: close-only frames; Go to time and About restyled; error dialog read-only details, focus on Retry; Effects tabs fit (narrow drawer moves More beside close), EQ scrolls; Open media Tab order tiles → panel → buttons.
- Library: Home updates the playing row in place (no rebuild); recent lengths and paths; no invisible Tab stops.
- Performance: spectrum idles while covered or minimised; reduced motion honoured by the seek loading bounce, art fade and snow.
- Translations: new sources in `po/POTFILES.in`; tooltips "%1 (%2)" translatable with localized key names; locale decimal separators.

### Deviations
- Effects tab and Preferences section UIA "Select" does nothing (Qt 5.6 `accSelect` is a stub); Invoke works.
- Moving the window to another monitor during playback can leave it taller (pre-existing; resizing to Windows' suggested rect would change behaviour).
- High contrast keeps system point fonts, so text may clip after a monitor move (`shortcut:` in `Theme::setScreen`).
- Focus ring and painted line widths are not scaled per monitor (token says unscaled).
- "Display preview" for snapshots (`snapshot-preview`) is shown but ignored: the toast replaces it (P6-9).
- The fullscreen controller is now reachable with Tab (stock: not a Tab stop); Esc still leaves fullscreen at once (owner decision P6-11).
- 100 % and monitor-move results come from the two-monitor runs before the owner's second screen was disconnected. The later per-monitor fixes (column widths, dialog minimums, rescale state on objects, `Theme::pt` fonts, following `windowHandle()->screen()`) were only exercised with a scale-change test hook, never on a real second monitor, and nothing after that was run on a true 100 % desktop (`QT_SCALE_FACTOR=0.8` breaks stock 3.0 too, so it can't stand in).
- Narrator was never run: screen-reader results come from UI Automation (names, roles, values, Invoke) only.
- Not run on Windows: animations off, high contrast at runtime, RTL, a monitor unplug.
- The harness `baseline` state `04-effects` doesn't show the sheet at the default size (it opens as the XS drawer window, which the capture misses).
- A Windows Firewall prompt ("allow VLC media player") appeared once during testing; it was cancelled, so no rule allows VLC. Tests only listen on 127.0.0.1.
- The mouse wheel changes the volume only over the video, the deck and the Now Playing bar (owner decision P6-10).
- RTL layout could not be run (needs a Windows setting); mirrored by code reading only.

### Deferred (pre-existing, stock)
- Open media Capture: the DirectShow device scan blocks forever on this machine (dump: dshow → devenum → FrameServerMonitorClient RPC wait).
- Alt+D shared by Disc and Capture Device (a translated string).
- 10,000-item playlist load (~45 s, stock ~40 s).
- Translations: this build has no NLS (`build.sh -l` needed).

### Manual test steps
1. Play a video, press Tab: focus moves through the deck; Shift+Tab from Pause reaches "Back to library", then wraps to the deck's end.
2. Leave the mouse on the video for 10 s: the header stays hidden; a 1-px move brings it back; pause shows it.
3. Ctrl+Shift+O, Shift+Tab to the File tile, Right: Disc is selected (no hang); Capture is never reached by Shift+Tab.
4. Home → Videos → Music → Network → Ctrl+P → Back at 1280×800: the window size never changes.
5. Move the window to a monitor with another scale and back: text, icons and rows grow together and come back identical; paused time stays.
6. Effects at 800×600: tabs show full labels, all 10 bands reachable by scrolling.

### Edge cases (`redesign/docs/EDGE_CASES.md`), Phase 6
Evidence: `D:\Projects\VLC\spike-results\p6-edge\` (runtime sessions), `p6-va\`, `p6-vb\`, `p6-vc\`, `p6-v2a\`, `p6-v3a\`, `p6-v3b\`, `p6-design\`, `p6-final\` and each fix lane's folder. Not committed.

| Row | Result | Evidence or reason |
|---|---|---|
| 800×600 minimum, maximised, snaps | **fixed** | With video the window reached only 882 logical px; the expert row hides first and the extra 80 px are gone. |
| DPI change while running, monitors with different scales | **fixed** | P6-6: the theme is re-applied at the new scale (pixel fonts, rescaled pages, caption cut per monitor dpi, column widths and dialog minimums kept). Two-monitor runs before the second screen was disconnected; later fixes with a test hook. |
| Light/dark switch, high contrast | **pass** / **deferred** | Theme switch with Help, About and pop-ups open passes. High contrast is still read at start only (`shortcut:`). |
| Animation effects off | **pass by code** / **not tested** | Every animation reads the setting (seek bounce, art fade, snow, spectrum, header). Running it needs a Windows setting. |
| Menu bar / minimal view / fullscreen ± controller | **fixed** | Ctrl+H restores width and sidebar; no header in fullscreen or minimal view; no invisible Tab stops in fullscreen; the controller is reachable with Tab. |
| Playlist docked / detached / XS drawer; no width drift | **fixed** | Docked panels never push the window past the work area; a small video keeps the panel docked; the resume panel no longer grows the window on every replay. |
| Toolbar strings, editor round-trip | **pass** | `fitWidth` only shows or hides; strings untouched; rebuilt toolbars get the current position. |
| `-I skins2`, `--qt-minimal-view` | **pass** | Effects window close-only; no header in minimal view; reopening the Qt interface is not possible in stock. |
| Long strings, RTL, CJK | **deferred** | RTL needs a Windows setting; mirrored by code only. Effects tabs elide with tooltips. |
| Keyboard only | **fixed** | Tab works after media opens (native-window fix); Open media tiles are a tab list; Shift+Tab wraps through the header back button without loops in every layout. |
| Space on a focusable button = play/pause | **pass** | Including the header back button (earlier failures were a test tool dropping spaces). |
| Narrator names and states | **fixed** (UI Automation; Narrator not run) | 0 unnamed controls on every surface; sliders, tiles, tracks button, seek bars ("x of y") and deck shortcuts exposed; duplicate caption buttons hidden (P6-3). UIA Select stays a Qt 5.6 stub. |
| Toast must not cover the subtitle it announces | **fixed** | The toast follows the immersive lift. |
| Subtitle position hotkeys while lifted; vout reuse | **pass** | Offset kept after the bar sleeps; no creep across items. |
| Two video outputs (clone) | **deferred** | No embedded vout in that mode, so the lift does not apply. |
| Buffering forever / unreachable URL | **fixed** | The ring shows for slow streams over the video's black; unreachable URLs show the error dialog with focus on Retry. |
| Aspect ratios | **fixed** | The badge shows the picture's ratio (16:9, also for 640×360). |
| Home → Videos → Home, Preferences: no size change | **fixed** | Library pages and Preferences never resize the window. |
| Recents length while playing | **fixed** | Updated in place, no page rebuild (focus kept). |
| Prefs Save/Cancel/Reset; stop after Preferences | **pass** / **fixed** | Stopping a video after Preferences now goes Home; the docked playlist hides while Preferences shows. |
| Open media: arrow keys, Capture | **fixed** / **deferred** | Arrow keys no longer reach Capture by accident; Capture itself still blocks in the DirectShow scan (stock). |
| Closing during playback, 50× open/close | **pass** | Final build, 50 cycles each (Preferences, Effects docked and XS, Open media, playlist, Media info, Go to time, About): handles, GDI and USER flat. The docked Effects sheet's private bytes rise 1.5 MB by cycle 25 then level off over 150 cycles (heap churn from resizing the video area; stock's separate Effects window doesn't resize the video). Exit code 0 in 0.2–0.6 s with each surface open; with a modal dialog (Open media, About) the main window ignores close, as in stock. |
| Overlays over the video after the native-window change | **pass** | Docked and XS panels, fullscreen controller, minimal view, mini player, audio mode, resize, dark and light. |
| Mouse wheel over panels | **fixed** | The wheel over the Effects sheet or playlist no longer changes the volume. |
