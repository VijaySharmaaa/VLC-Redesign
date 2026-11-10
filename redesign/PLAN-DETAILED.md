# Detailed plan: VLC 3.0.x Windows redesign

Builds on `redesign/ARCHITECTURE.md` (what exists), `redesign/docs/V3_IMPLEMENTATION_PLAN.md` (agreed adaptations) and the spike results in `PROGRESS.md`. Each phase is a branch `redesign/phase-N` with small commits that each build. Every phase ends with the verification list in section 9.

## 0. Decisions taken provisionally (owner to confirm)
| # | Decision | Default taken | Reversible by |
|---|---|---|---|
| D1 | App icon (1a / 1b / 1c) | **Keep the current icon.** Only the in-app UI cone (`vlc-ui-cone-flat.svg`) is used in the title bar. | One commit swapping `share/icons` |
| D2 | Shift+drop = enqueue | **Not added** (changes behaviour). The drop target says "Drop to play". | Add a `keyboardModifiers()` check in `dropEventPlay` |
| D3 | Mini player shortcut | **Ctrl+H stays minimal view.** The mini player gets a View menu entry and a deck button, with no shortcut. | Add a `key-*`-style accelerator |
| D4 | Icon glyphs | **Material Symbols Rounded** (Apache-2.0) as an SVG subset in `modules/gui/qt/pixmaps/redesign/`, with its licence. | Replace the SVG files |
| D5 | Fonts | **System fonts only**: Segoe UI Variable Text → Segoe UI; mono: Cascadia Mono → Consolas. Nothing bundled. | n/a |
| D6 | Library data | **No new media database.** Home = recents; Videos/Music = the existing `video_dir`/`audio_dir` services-discovery modules; Network = the SD categories. `media-library` stays off. | n/a |
| D7 | Mica | Only behind the title bar (and sidebar), never behind video, and only if snap layouts survive (spike a vs c). Otherwise solid `--chrome`. | Runtime check |

## 1. Theme and QSS architecture (Phase 1, used by every later phase)
- **`util/theme.hpp/.cpp`** (new) holds the tokens (from `redesign/design/v3/redesign_tokens.hpp`, copied in as `util/theme_tokens.hpp`) and exposes:
  - `Theme::instance()`, `Theme::isDark()`, `Theme::palette()` (the token `Palette` for the current mode).
  - `Theme::color(Role)`, `Theme::ink(alpha)`.
  - `Theme::font(TypeRole)`, `Theme::metric(SizeRole)`, `Theme::duration(MotionRole)`; `duration()` returns 0 when Windows animations are off (`SPI_GETCLIENTAREAANIMATION`).
  - `Theme::icon(name, colourRole)`: renders an SVG from `:/redesign/<name>.svg` tinted with a token colour, cached per (name, colour, size).
  - `Theme::apply(QApplication&)`: sets the Fusion style, the `QPalette`, the app font and the **generated** application stylesheet.
  - A `changed()` signal.
- **QSS is generated** in one function from tokens (no `.qss` files with literals): menus, menu bar, buttons, tool buttons, line edits, combos, sliders, scroll bars, tabs, tooltips, check/radio, group boxes, and `[role="…"]` property selectors for redesign widgets.
- **Mode** comes from the new option **`qt-redesign-theme`** (int: 0 = follow Windows, 1 = dark, 2 = light; default 0). It follows `AppsUseLightTheme` (HKCU registry) and updates live on `WM_SETTINGCHANGE "ImmersiveColorSet"`, handled in the existing native filter. High contrast (`SPI_GETHIGHCONTRAST`) → system palette, no QSS.
- **`qt-dark-palette`** stays declared. When `qt-redesign-theme` = 0 and `qt-dark-palette` is on, dark is forced, so old configs still get dark. `applyDarkPalette` and the static cache in `isDarkPaletteEnabled` are removed; callers use `Theme`.
- **Title bar:** `util/win_theme.hpp` from spike c (runtime `DwmSetWindowAttribute`: dark mode 19/20, backdrop 38, caption colour 35) is applied to every top-level window through the `QVLCFrame`/`QVLCDialog`/`QVLCMW` constructors and on theme change.
- **Applied in `ThreadPlatform` before `MainInterfaceWin32` is created** (`qt.cpp:632`). skins2 dialog-provider mode gets the palette and QSS too (dialogs only; skins2 itself isn't Qt).
- `MainWindow/QtStyle` keeps working: a user-chosen style replaces Fusion, and the QSS still applies.

## 2. Phase 1 — Theme foundation
**Goal:** the whole app is restyled from tokens with no layout changes.

**Files**
- New: `util/theme.hpp/.cpp`, `util/theme_tokens.hpp`, `util/win_theme.hpp`, `pixmaps/redesign/*.svg` (+ `LICENSE-material-symbols.txt`).
- Changed: `qt.cpp` (option, apply order), `util/qvlcframe.hpp`, `main_interface_win32.cpp` (`WM_SETTINGCHANGE`), `vlc.qrc`, `Makefile.am`.
- Hard-coded styling replaced: `dialogs/help.cpp`, `main_interface.cpp:576-586`, `components/interface_widgets.cpp:958`, `components/playlist/selector.cpp:431`, `util/customwidgets.cpp`, `util/searchlineedit.cpp`, `styles/seekstyle.cpp`, `util/input_slider.cpp` (colours only), `util/timetooltip.cpp`, `ui/about.ui`, `ui/open_net.ui`, `ui/sprefs_interface.ui`.
- Simple Preferences "Use a dark palette" becomes a Theme selector (follow Windows / dark / light) bound to `qt-redesign-theme`.

**Inventory:** 3.7 (light theme, structure only), all menus 2.4 (restyle). **Edge cases:** Windows 10 vs 11 / missing DWM; light/dark switch while running; high contrast; animations off; skins2 untouched; clean vs old ini; translations (no clipping in restyled menus).

**Risks:** Fusion plus QSS may change widget metrics in `.ui` dialogs, so check every dialog for clipping. QSS on `QSlider` conflicts with the custom-painted seek slider: exclude it with a property selector.

## 3. Phase 2 — Window shell
**Goal:** frameless 40 px title bar with the menu inside, a sidebar, a content stack and breakpoints.

**Files**
- New: `components/title_bar.*`, `components/sidebar.*`, `components/toast.*`, `components/drop_overlay.*`.
- Changed: `main_interface.cpp/.hpp`, `main_interface_win32.cpp/.hpp`, `menus.cpp`.

**Title bar** (spike a): `WindowsCustomMargins` caption removal and `WM_NCHITTEST` on the main HWND only. The menu bar is created inside the title bar (`setMenuWidget`). Menu shortcuts are registered on the window so minimal view and fullscreen can hide the bar safely. Caption buttons are 46 px; close hover `#c42b1c`. Every `menuBar()` call is updated (:318, 849-850, 1131, 1145-1183, `menus.cpp:332`). Minimal view hides the menus *and* collapses the bar to the caption buttons. Snap-layout hover: `HTMAXBUTTON`.

**Sidebar:** "Open media" (accent) plus Home, Videos, Music, Network, and Preferences at the bottom. It collapses to an icon rail below 1200 px and is hidden in minimal view and fullscreen. It drives `stackCentralW` through new library tabs (Phase 4 fills them; Phase 2 shows the existing bg/video/playlist plus placeholders).

**Breakpoints:** a `resizeEvent` sets a `breakpoint` property (L/M/S/XS) that widgets read, and QSS selectors re-polish.

**Toasts** and the **drop target** (dashed accent frame, "Drop to play") are top-level overlay windows (spike b pattern) owned by `MainInterface`.

**Inventory:** 1.1.2 (drop), 1.6.1, 2.4, 3.1-3.5 (layout), 2.6.1 (toast host). **Edge cases:** min size 800×600, maximise/snap/restore, Alt+Space, Win+↑/↓, taskbar positions, DPI change across monitors, minimal view, fullscreen with the FSC on/off, systray, taskbar buttons, `-I skins2`, `--qt-minimal-view`, keyboard Alt for menus.

**Risks:** `resizeStack`, `setVideoSize` (`PM_TitleBarHeight`) and `setMinimalView` maths; snap detection with the new frame; the taskbar buttons after a flag change. **Fallback:** if `WindowsCustomMargins` misbehaves on Windows 10, keep the native caption and put the menu row under it.

## 4. Phase 3 — Player
**Goal:** the docked deck, overlays on the video, popovers, the playlist panel, immersive mode, now-playing bar, mini player and audio mode.

**Deck**
- New default strings. `INPT_TB_DEFAULT` stays `"43;33-4;44"` (elapsed · seek · remaining). `MAIN_TB2_DEFAULT` becomes play (BIG) · prev · stop · next | loop · random | advanced controller | tracks · speed · effects · playlist · mini player · fullscreen | volume, with exact numbers fixed in the phase after checking the editor. `MAIN_TB1_DEFAULT` keeps the teletext buttons only. Existing saved strings are untouched.
- New `buttonType_e` values appended before `BUTTON_MAX`: **`TRACKS_BUTTON` = 26**, **`MINIPLAYER_BUTTON` = 27** (`BUTTON_MAX` → 28), each with `nameL`/`tooltipL`/`iconL` entries, actions and editor support. The new `FSC_TB_DEFAULT` mirrors the deck.
- Buttons are 36×36 icon buttons with radius 6 (via QSS `[role="deck"]`); play is a 44 px accent circle. Seek and volume are repainted from tokens in `SeekStyle`/`SoundSlider` (4 px track, 14 px knob with halo, chapter gaps, 125 % boost colour, tick at 100 %).

**On the video and in popovers**
- **Overlays** (spike b): one host tracking `VideoWidget`, with one top-level window each for the paused badge, buffering ring + %, toast and immersive bar. Drops on the overlays are forwarded to `dropEventPlay`.
- **Popovers:** tracks (audio | subtitles, + "Add subtitle file…") and speed (8 presets) as `Qt::Popup` windows, using the existing input variables (`audio-es`, `spu-es`, `rate`).

**Panels and modes**
- **Playlist panel:** a 40 px row delegate and filter field on the shared `PLModel` inside `PlaylistWidget`, plus the existing context menu (restyled; the destructive item is red), with widths L 360 / M 320 / S 300 and an overlay drawer at XS. The detached playlist window keeps working.
- **Immersive layout:** the floating bar auto-hides after 2.5 s and hides the cursor; it replaces nothing (the FSC stays for video fullscreen).
- **Now-playing bar** (76 px) in library views. **Mini player:** a small frameless always-on-top window.
- **Audio mode:** art, track header, tracklist and the spectrum (spike d's `qt_spectrum`).

**Inventory:** 1.1.6, 1.2.1-1.2.7, 1.3.1-1.3.4, 1.4.1-1.4.5, 1.6.3-1.6.4, 2.1.1-2.1.2, 2.2.2-2.2.5. **Edge cases:** idle and no media, live streams, short/long media, chapters, audio-only, no tracks / 20+ tracks, speed extremes, volume 0/100/125 and `qt-max-volume` 200/300, mute, buffering forever, saved old toolbar strings, empty/corrupt strings, toolbar editor round trip, rapid clicks, 10,000-item playlist.

**Risks:** saved toolbar layouts look old (by design). The FSC's thread race (`mouseChanged`) must not be copied.

## 5. Phase 4 — Library
**Files:** new `components/library/*` (home, grid view, network view, empty state, search).

**Content**
- **Home:** continue watching (recents with `time > 0`) plus a recent-media table.
- **Videos/Music:** grids on their own `PLModel` instances rooted at the `video_dir`/`audio_dir` SD nodes. Art loads asynchronously, off the paint path (fixes the synchronous `QImageReader` in paint). Columns L 5 / M 4 / S 3 / XS 2.
- **Network:** SD categories plus Streams (open network stream, Icecast, podcasts).
- **Empty state:** "Add a folder" (adds to the SD's folder list) and "Open media…".
- **Search** in the content header.

**Inventory:** 1.1.1, 1.1.4, 1.1.5, 2.2.1, 2.3.1. **Edge cases:** empty library, 10,000+ files, deleted/offline recents, removable drives, UPnP appearing and disappearing.

**Risk:** SD loading is lazy, by name, and never unloaded. Reuse `PLSelector`'s loading code rather than duplicating it.

## 6. Phase 5 — Sheets and dialogs
**Approach:** restyle the existing dialogs with tokens and new layouts. Keep `showTab`/`currentTab`, Save semantics, `QVLCFrame` Esc/Enter and saved geometry. Embed in the window only where all of that survives.
- **Effects:** a docked sheet over the right of the video with tabs Equalizer · Video · Synchronization, preset chips, 10 bands ±20 dB, preamp, video sliders + reset, sync steppers ±50 ms. Save behaves as before.
- **Open media:** one sheet with tab tiles File · Disc · Network · Capture, a mono URL field and a recent-streams list.
- **Preferences:** in-window view. The section list with a card of rows is built on the existing Simple Preferences panels; "Show all settings" opens the existing complete tree.
- **Media information** (tabs restyled), **Go to time** (mono input), **Error** (Show log + Retry), **About**.

**Inventory:** 1.1.3, 1.3.5, 1.5.1-1.5.4, 1.6.2, 1.6.5, 1.7.1-1.7.2, 2.1.3-2.1.6, 2.5, 2.6.2. **Edge cases:** prefs Save/Cancel/Reset writing the same variables, old configs, portable mode, dialogs closed during shutdown, open/close 50 times, translations.

## 7. Phase 6 — Polish
Light theme on every screen (3.7); XS behaviour (3.6); keyboard focus rings, tab order and accessible names on every control (the baseline had 9 unnamed controls); reduced motion; HiDPI 100/125/150/200 %; Windows 10 and 11; a full screenshot pass against `references/` for every inventory row.

## 8. New options and keys (all phases)
| Name | Kind | Phase |
|---|---|---|
| `qt-redesign-theme` | int option, 0 follow / 1 dark / 2 light | 1 |
| `qt-immersive` | bool option, immersive layout in windowed mode (default off) | 3 |
| `MainWindow/sidebar-rail` | QSettings bool (user-collapsed rail) | 2 |
| `MiniPlayer/geometry` | QSettings | 3 |
| `TRACKS_BUTTON` = 26, `MINIPLAYER_BUTTON` = 27 | `buttonType_e` | 3 |

## 9. Verification for every phase
1. **Build:** `redesign/tools/qt-rebuild.sh` (full `make` when anything outside `modules/gui/qt` changes). There must be no new warnings in changed files.
2. **Run:** `harness.py` (portable profile) with `-vvv` logging; the Qt module must load cleanly.
3. **Screens:** reproduce the phase's inventory states, screenshot them, compare with `redesign/references/*.png` and log differences in `PROGRESS.md`. Check dark and light, 100 % and 150 %, and widths 1920 / 1280 / 1024 / 800.
4. **Regressions:** toolbar editor, minimal view, FSC, systray, taskbar buttons, detached playlist, every hotkey, `-I skins2`.
5. **Names:** `harness.py names` shows no new unnamed controls.
6. **Reviews:** code review of the diff, edge-case rows recorded (pass / fixed / deferred with reason), and a final readiness review.

**Not available here:** Windows 10 and high-DPI monitors aren't on this machine. Windows 10 paths are tested by forcing the fallback; 150 % is tested with `QT_SCALE_FACTOR=1.5`, which approximates it but isn't the same as system scaling.

## 10. Design update: Preferences uses one sidebar (2026-11-10)
Source: `DESIGN_SPEC.md` §8, `SCREEN_INVENTORY.md` 2.5.P1–2.5.P6, references `11-flows-library-menus-settings__2.5.*`. The in-window Preferences page (Phase 5) is already built, so this changes existing code. **Owner decision: Save / Cancel stays**; the "Changes apply immediately" line is not added.

**Change set** (no new files; no QSettings keys involved):
| File | Change |
|---|---|
| `components/sidebar.cpp/.hpp` | A `QStackedWidget` inside the sidebar: library navigation \| preferences navigation (back row 32 px, "Preferences" heading 22/600, the 7 sections as the existing `SidebarButton` items, 36 px / 20 px icons, in their own exclusive group). Swapped in `Sidebar::setCurrentWidget` when the Preferences page becomes current, which every entry path already reaches. The active item gets a 3 px accent bar (inset 9 px) painted from tokens. Rail mode (< 1200 px) reuses `setRail`: back arrow + section icons with tooltips, heading hidden. The sidebar remembers the page it left (`currentPage`) as the return target. |
| `components/prefs_page.cpp/.hpp` | Its own section list, rail handling and resize code go; `showSection` becomes public and is driven by the sidebar (one source of truth). The section name becomes the page title (26/600); Advanced's duplicate title goes. Tab order: back row → sections → content. |
| `main_interface.cpp` | Esc goes back (after the Effects sheet and before minimal view in the existing `handleKeyPress` chain; menus, combo pop-ups and dialogs close first because they're separate windows). `showPreferences` falls back to the dialog in minimal view too (the sidebar is hidden there). Back to a video that ended meanwhile goes to the art/Home page instead of a blank view. |
| `util/theme.cpp`, `util/theme_tokens.hpp` (+ `design/tokens.json`, `design/v3/redesign_tokens.hpp`) | New tokens: back row 32, heading 22/600, accent bar 3 / inset 9 / radius 2. Dead `prefsList`/`prefsSections` QSS and `Prefs::List*` tokens removed. |

**Behaviour:** Back/Esc return to the view Preferences was opened from (library page, player or audio mode) — playback, the playlist panel and library scroll positions are untouched because only the content stack switches. The back label reads "Back to library" for a library page, else "Back to player". Ctrl+P, Tools › Preferences and the dialog fallback (minimised, fullscreen, mini player, `qt-advanced-pref`) are unchanged. RTL: the back arrow glyph is mirrored. Accessibility: the active section is the checked radio item (Qt 5.6 has no "current" state; UIA reports selection), verified with pywinauto.

**Verification:** 2.5.P1–P6 in dark and light at 1920/1280/1024/800, 100 % and 150 % (`QT_SCALE_FACTOR=1.5`); edge cases: open during playback and return (playback and deck unchanged), from minimal view and the separate playlist window, fast section switching, a long section name (ellipsis), RTL (`--qt-…` layout direction / `-reverse`), Narrator names of the back row and current section, Esc with a select drop-down open. Then code review, design review, accessibility audit and the readiness review.
