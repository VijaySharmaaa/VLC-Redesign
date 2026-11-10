# Edge-case checklist

Check every changed area against the relevant rows before a phase is closed. Record the result in PROGRESS.md as pass / fixed / deferred (with a reason).

## Window & display
- [ ] Window at its minimum size of 800 × 600; maximised; snapped half and quarter (Win 11 snap layouts); restored after maximise.
- [ ] Dragging between monitors with different DPI (100 % ↔ 150 % / 200 %); changing scale while running.
- [ ] Multi-monitor fullscreen (`qt-fullscreen-screennumber`); unplugging the monitor VLC is on.
- [ ] Windows 10 vs 11 (Mica present / absent); Windows 7/8.1 if the build claims support. No crash when DWM APIs are missing.
- [ ] Switching Windows light/dark while VLC is open; high-contrast mode (fall back to system colours, keep it readable).
- [ ] "Animation effects" off: no motion, and state changes are still visible.
- [ ] Taskbar on top/left/auto-hide (maximised window must not cover it); title-bar double-click, Alt+Space system menu, Win+↑/↓.
- [ ] Quarter snap (Win+Left, Win+Up) on a 1920×1080 work area: a quarter is shorter than the 600 px minimum height.
- [ ] Restoring a saved maximised `geometry` whose normal size is below the current minimum (stock 3.0 default 600×420): must come back truly maximised, not "zoomed" at the minimum size.
- [ ] Leaving video fullscreen by end of media (not Esc/f) puts the window back at its old position.
- [ ] Touch and pen input on the title bar, seek and volume.

## Interface states
- [ ] Menu bar hidden or shown (Ctrl+H in v3 minimal view); minimal view; fullscreen with the fullscreen controller on and off (`qt-fs-controller`).
- [ ] Playlist docked vs detached window; playlist panel at XS (overlay drawer).
- [ ] Docked playlist toggled (Ctrl+L) repeatedly across the sidebar rail breakpoint, and in minimal view: window size must not drift.
- [ ] Saved custom toolbar strings from an old VLC (must still load); empty or corrupt toolbar strings fall back to the defaults; the toolbar editor round-trips.
- [ ] First run (privacy dialog `qt-privacy-ask`); a clean profile vs a profile with old `vlc-qt-interface.ini`.
- [ ] Systray on/off, start minimised, pause on minimise; taskbar thumbnail buttons.
- [ ] `TaskbarButtonCreated` arriving more than once: thumbnail buttons and their signal connections are created once.
- [ ] `-I skins2` and `--qt-minimal-view` still work; skins2 isn't affected by the theme.
- [ ] Translations: long German/Russian strings in menus and buttons (no clipping), right-to-left (Arabic/Hebrew) layout mirroring, CJK fonts.
- [ ] Keyboard only: tab order, focus visible on every control, Esc closes the top popover/dialog first, hotkeys don't fire while typing in a field.
- [ ] Tab order through the deck follows the visual order (special widgets such as speed and volume must not come before the button groups).
- [ ] Space with ANY focusable button focused (deck, mute, now-playing bar, audio-mode "Library" pill, mini player) toggles play/pause and doesn't activate the button.
- [ ] The deck's minimum width while playing still lets the window reach 800 px (the XS drawer must be reachable).
- [ ] Toasts are sized to their text: no elision while the content area has room.
- [ ] Overlay copies of the deck (immersive bar, FSC) show the same mute/volume state as the docked deck when they are created.
- [ ] Screen reader (Narrator): every icon button has a name, and state changes (play/pause, mute) are announced.
- [ ] Every combo box drop-down opens at a normal size with all items visible and clickable, in each theme and with each `MainWindow/QtStyle` (Fusion, Windows, an unknown name).
- [ ] Focus indicator on check boxes and radio buttons has at least 3:1 contrast against the window, in light and dark.
- [ ] Translations need a build with NLS (`build.sh -i`); a `--disable-nls` package shows English whatever `LANG` says.
- [ ] Theme switch while running in skins2 / dialog-provider mode (no Qt main window receives `WM_SETTINGCHANGE`).
- [ ] Simple Preferences saved with "Force window style: System's default": the next start must keep the themed look, not fall back to the Vista style.
- [ ] A side panel (Effects sheet, docked playlist) that has once been the XS drawer, back at S or wider: it must paint again (open while resizing, or reopened after), not stay a blank native child.
- [ ] Esc in video fullscreen with the Effects sheet open: closes the sheet first, then leaves fullscreen.
- [ ] Ctrl+P (and other menu shortcuts) while only the mini player is shown: Preferences must stay reachable.
- [ ] Player header in the normal layout: hides with the mouse cursor while a video plays and comes back on a mouse move, pause or stop, and Shift+Tab to its back button; it never stays over top-positioned subtitles.
- [ ] Immersive layout turned off while its bar is up: the space kept for the bar (subtitle lift, audio page bottom) is given back at once.
- [ ] Qt interface closed and opened again in one process (`-I skins2`, then the Qt interface): the accessibility overrides (now-playing seek, title bar, Open media tiles) still apply.
- [ ] Open media: an Alt mnemonic shared by two tiles (Alt+D, Disc and Capture Device) focuses the unchecked tile without opening it; Tab from the checked tile goes into its panel.
- [ ] The deck's expert row hidden by the window width while Advanced Controls is on: its controls stay reachable and come back when the window widens.
- [ ] Help open while the theme switches: the text keeps its scroll position.
- [ ] VLC's interface language differs from the Windows locale: numbers, times and dates follow one of them consistently, not a mix.

## Media & playback
- [ ] Nothing loaded (idle): controls disabled sensibly, no division by zero in time/seek code.
- [ ] Live streams and unknown length: seek disabled, "Live" label, no negative remaining time.
- [ ] Seekability learned after the input opens (`can-seek` starts true and is fixed by the demuxer): the seek knob and the now-playing bar seek must follow it, without waiting for a title/chapter event.
- [ ] After the media ends or stops, the now-playing bar's seek is disabled (no stale `inputCanSeek`).
- [ ] Test media folders: a `cover.jpg`/`folder.jpg` next to the files is used as art by VLC's folder art finder, so keep "no art" files in a folder without one.
- [ ] Very short (< 1 s) and very long (> 10 h, h:mm:ss) media; media with and without chapters (chapter ticks).
- [ ] Audio-only, video-only, no art, huge art, transparent PNG art.
- [ ] No audio tracks, no subtitle tracks, 20+ tracks (popover scrolls), tracks added mid-playback (an .srt dropped in).
- [ ] A toast (e.g. "Loaded subtitle") must not cover the subtitle line it announces.
- [ ] A toast showing while the immersive bar comes up or hides: it moves with the lifted subtitles.
- [ ] Subtitle position hotkeys (`key-subpos-up` / `key-subpos-down`) while the subtitles are lifted over the immersive bar: the user's offset stays after the bar hides.
- [ ] Two video outputs (clone filter, a second video track in its own window): only the one under the immersive bar has its subtitles lifted.
- [ ] A video output reused by the next item: the subtitle lift doesn't add up (creep) from item to item.
- [ ] Go to time while the media changes with the dialog open, and with media longer than 24 h.
- [ ] Aspect badge for a video whose size only the decoder knows (no size from the demuxer).
- [ ] Speed at the extremes (0.25×, 4×, and values typed via hotkeys that aren't presets).
- [ ] Volume 0, 100, 125, and `qt-max-volume` set to 200/300 (tick and boost colour scale correctly); mute → unmute restores the level.
- [ ] Buffering that never finishes; the network dropping mid-stream; an unreachable URL (error dialog, Retry works, Show log opens messages).
- [ ] DVD/Blu-ray menus, teletext, 360°/VR video, interlaced video, HDR, rotated phone video, 4:3 / 21:9 / 1:1 aspect ratios.
- [ ] Opening from Explorer while VLC is running with "one instance" on (enqueue vs play); command-line files; drag-and-drop of files, folders, URLs and `.srt` (Shift = enqueue).
- [ ] Unicode, emoji, very long and UNC (`\\server\share`) paths and titles: no crash, sensible ellipsis.

## Library & playlist
- [ ] Empty library (no Videos/Music folders, permissions denied); a folder with 10,000+ files (no UI freeze; lazy loading).
- [ ] Playlist with 10,000 items: scrolling, filter, sort, remove, current-item tracking.
- [ ] Recent media pointing at deleted files or offline network shares.
- [ ] Removable drives and discs inserted or removed while browsing; UPnP servers appearing and disappearing.
- [ ] Switching library pages (Home → Videos → Home) on an L-size window: the window width must not change (stack sizes remembered per page vs. the sidebar rail breakpoint).
- [ ] A Continue-watching card resumes with `:start-time`; replaying the same playlist item later (Stop → Play, double-click in the playlist) must not jump back to that old position.
- [ ] Recents config shared with stock VLC 3: stock rewrites `RecentsMRL/list`/`times` but not `lengths`/`dates`, so the parallel lists can shift against each other.
- [ ] A failed open of a recent item must not overwrite its known length with 0.
- [ ] Quit while a slow internet discovery (Icecast directory, podcasts) is still loading: VLC must exit within a few seconds; the list shows a loading state, not a blank page.
- [ ] Quit while the Home page's existence check is blocked on an offline share: Qt waits for the global thread pool at exit.
- [ ] First visit to the Network page loads every LAN discovery (UPnP, mDNS, SAP) at once, so the Windows Firewall prompt appears then and never at startup; Home/Videos/Music must not load them.
- [ ] Files added to a Videos/Music folder while VLC runs (VLC 3's media directories don't watch folders: shown only after a restart).

## Settings & robustness
- [ ] Preferences: Save, Cancel, Reset; simple ↔ all settings; values written to the same config variables as before; old config files still load.
- [ ] Portable mode (`--config` / settings next to the exe).
- [ ] Rapid repeated clicks (play/pause spam, opening/closing panels quickly): no stuck animations or leaked widgets.
- [ ] Closing VLC during playback, during buffering, or with a dialog open: clean shutdown, no crash, settings saved.
- [ ] Memory: open and close every panel and dialog 50 times; no growth (check with Process Explorer or a debug build).
- [ ] Preferences page with panels already built while the same options are saved elsewhere (the Ctrl+P dialog fallback in fullscreen, Show all settings, Reset, the Effects sheet's Save): the page's next Save must not write its stale values back.
- [ ] Errors re-enabled after "Hide future errors" (Reset, or the option in all settings): the next error dialog shows the box unchecked.
- [ ] Open media "Show more options" kept across a restart (`OpenDialog/advanced`), also when VLC quits with the dialog open or closed.
- [ ] Open media at XS (800 × 600) with "Show more options": the dialog stays on screen and usable.
- [ ] Accessible names on dialog fields: the Open media network URL, the Media information metadata fields.
- [ ] Translated preferences: an option description whose first line is a lead-in ending in a colon, also the full-width colon (：) of CJK translations, is kept with the text that follows.
