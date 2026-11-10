# Architecture of the VLC 3.0.x Qt interface (Windows)

How the parts the redesign touches fit together. All paths are relative to `modules/gui/qt/`. Line numbers are from `3.0.x` at `c128d1f15c`.

## 1. Main window shell

### Construction (`main_interface.cpp:95-266`)
1. `FirstRun::CheckAndRun` (:118), then options are read (:132-153).
2. `createMenuBar` (:161), `createMainWidget` (:165), status bar (:173-174), `initSystray` (:180).
3. Signal wiring (:190-240), then variable callbacks `intf-toggle-fscontrol`, `intf-boss`, `intf-show`, `intf-popupmenu` (:246-251).
4. The playlist is toggled if it was visible (:255), geometry restored (:258), `setVisible(!b_hideAfterCreation)` (:262); minimal view comes after the show (:265).

### Widget tree (`createMainWidget`, :462-536)
The central widget's `QVBoxLayout` holds: resume panel, `stackCentralW` (index 1), `inputC` (index 2) and `controls` (index 3, or 0 when `MainWindow/ToolbarPos` is set). `recreateToolbars` re-inserts at these fixed indices (:333-338). The stack holds:
- `bgWidget`: the cone or album art (:475-482).
- `videoWidget`: created up front only with `embedded-video` (:493-497), otherwise lazily in `getVideoSlot` (:743-747).
- `playlistWidget`: only when docked (`createPlaylist`, :1027-1038).

The fullscreen controller (FSC) is created only if `depth()>8` and `qt-fs-controller` is on (:528-534).

### Views and modes
- **`showTab`** (:651-723) switches the stack. During playback it reparents `videoWidget` into `playlistWidget->artContainer` and back. With autoresize, `resizeStack` (`main_interface.hpp:237-249`) resizes the whole window to the size remembered per widget, except when fullscreen, maximised or tiled.
- **Minimal view**: `setMinimalView` (:1143-1196) hides the menu bar, controls, status bar and `inputC`, and changes the window height by theirs. Ctrl+H and Esc are hard-coded (:1667-1668).
- **Video fullscreen**: `setVideoFullScreen` (:883-938) honours `qt-fullscreen-screennumber`, hides the chrome (`displayNormalView`, :1129-1137), then calls `setInterfaceFullScreen(true)`.
- **Interface fullscreen**: `toggleInterfaceFullScreen` (:1732); F11 is hard-coded (`menus.cpp:523`). `changeEvent` (:1500-1551) restores the maximised state and handles pause-on-minimise.
- **Playlist**: `togglePlaylist` (:1040), `dockPlaylist` (:1083-1122); undocked it's the `PlaylistDialog` singleton.
- **Systray**: `initSystray` / `createSystray` (:541-557, :1328-1353).
- **Always on top**: from `video-on-top` at startup (:153, :535); the menu toggle `setInterfaceAlwaysOnTop` (:1004) isn't saved; the vout's request `setVideoOnTop` (:980).

### Settings (`vlc-qt-interface.ini` in the VLC config dir, INI format, `qt.cpp:583-598`)
| Key | Read | Written |
|---|---|---|
| `MainWindow/pl-dock-status` | :150 | :289 |
| `MainWindow/status-bar-visible` | :174 | :296 |
| `MainWindow/playlist-visible` | :255 | :292 |
| `MainWindow/bgSize` | :500 | :299 |
| `MainWindow/adv-controls` | :505 | :294 |
| `MainWindow/ToolbarPos` | :337, :509 | `dialogs/toolbar.cpp:280` |
| `MainWindow/playlistSize` | read without its prefix at :1035, so never restored (upstream bug) | :300 |
| `geometry` (root) | :258 | :304 |
| `MainWindow/QtStyle` | `qt.cpp:681` | |
| `filedialog-path` | `qt.cpp:678` | `qt.cpp:722-725` |

Options read: `qt-opacity`, `qt-video-autoresize`, `qt-minimal-view`, `qt-notification`, `qt-pause-minimized`, `video-on-top`, `qt-name-in-title`, `qt-fs-controller`, `qt-continue`, `qt-auto-raise`, `qt-icon-change`, `qt-bgcone`, `qt-bgcone-expands`, `embedded-video`, `qt-system-tray`, `qt-start-minimized`, `qt-fullscreen-screennumber` (:132-889), and `qt-disable-volume-keys` (`main_interface_win32.cpp:90`). `reloadPrefs` re-reads only three of them (:352-360).

### The video window and threads
- `WindowOpen` runs on a **vout thread** (`qt.cpp:766-846`). It takes the static `lock`, checks `active`, then calls `getVideo`, which emits `askGetVideo` over a `Qt::BlockingQueuedConnection` (`main_interface.cpp:203-208`, :729).
- `getVideoSlot` (:735-768) calls `VideoWidget::request`, which creates the child `stable` widget with `WA_NativeWindow` (+ `WA_PaintOnScreen` on Windows) and hands its HWND to the vout (`components/interface_widgets.cpp:126-158`). **Qt widgets can't paint over it; overlays must be top-level windows** (proved in spike b).
- `WindowControl` only emits queued signals (`main_interface.cpp:1021-1057`). Size reports come from `WM_SIZE` in an app-wide native filter (`interface_widgets.cpp:213-275`).
- `WindowClose` emits the blocking-queued `askReleaseVideo` (`qt.cpp:895-922`); `release()` calls `deleteLater` on `stable` (`interface_widgets.cpp:386-401`).
- Rule (comment at :720-726): Qt state changes only on the Qt thread.

### Windows-specific code (`main_interface_win32.cpp`)
- `MainInterfaceWin32` installs an **app-wide** `QAbstractNativeEventFilter` (:94) that never checks `msg->hwnd` (:222-326). New title-bar handling must gate on the main window's HWND.
- Taskbar thumbnail buttons: COM `ITaskbarList3`, created on `TaskbarButtonCreated` (:124-217), clicks via `THBN_CLICKED` (:237-254), updated by `changeThumbbarButtons` (:415-473).
- `WM_APPCOMMAND` (:255-323): media keys, honouring `qt-disable-volume-keys`.
- Snap detection: `resizeEvent` compares `rcNormalPosition` with the window rect to set `b_isWindowTiled` (:360-397).
- Dark title bar: `setImmersiveDarkModeAttribute` (`util/qvlcframe.hpp:49-76`), runtime-loaded `DwmSetWindowAttribute` (attribute 20, then 19), applied only with `qt-dark-palette` (:301-306).
- No `WM_NCCALCSIZE`/`WM_NCHITTEST` code exists yet.

### Lifetimes
- **Startup**: `Open` starts `ThreadPlatform` and blocks on `ready` (`qt.cpp:451-504`). The thread creates `QVLCApp`, `mainSettings`, `DialogsProvider`, `MainInputManager`, then `new MainInterfaceWin32` (:632). It sets the playlist's `qt4-iface` and `window` variables, posts `ready` (:663) and runs `exec()` (:690).
- **Shutdown**: `Close` → `playlist_Deactivate` → `triggerQuit` → `vlc_join` (:518-543). After `exec()`: the variables are destroyed, `active=false`/`p_mi=NULL` set under `lock`, `delete p_mi` (:693-705); then the Extensions/Addons managers, `DialogsProvider`, `RecentsMRL`, the playlist model, `MainInputManager` and finally `mainSettings` (:708-734).
- The destructor (`main_interface.cpp:268-314`) force-releases the video, deletes the FSC, saves settings and removes the variable callbacks. Don't use `p_mi` once NULL (`IntfShowCB` and friends, :1823-1855), or `getSettings()` after :734.
- **skins2** runs the Qt module in dialogs-provider mode with `p_mi == NULL` (`qt.cpp:629`), so main-window code isn't used there.

### Where the redesign hooks in
- **Custom title bar**: `nativeEventFilter`, gated on `WinId(this)`. The height maths assume a native frame and menu bar: `setVideoSize` subtracts `PM_TitleBarHeight` (:851-865); `setMinimalView` and `computeMinimumSize` use `menuBar()` (:318).
- **Sidebar + content stack**: `createMainWidget` / `stackCentralW`. `showTab` and `releaseVideoSlot` assume the three tabs and the `artContainer` reparenting (:678-722, :793-799).
- **Now-playing bar**: a new `mainLayout` row; adjust `recreateToolbars`' fixed indices.
- **Theme**: in `ThreadPlatform`, before `MainInterface` is built (today `QtStyle` and the dark palette are applied at `qt.cpp:681-688`, after it). The status-bar labels have inline QSS (:576-583).

### Open questions (to settle by experiment)
- Does `setWindowFlags` (:535, :990-994, :1008-1012) recreate the HWND and lose the dark title bar and taskbar buttons? Spike c saw always-on-top keep the same HWND.
- `WindowOpen` holds `lock` while waiting on a blocking-queued signal, and teardown takes `lock` after `exec()`. Is there a shutdown deadlock?
- `~MainInterfaceWin32` calls `CoUninitialize` even if `CoInitializeEx` never ran (`main_interface_win32.cpp:103`).

## 2. Playlist, recents, menus, dialogs provider

### Playlist
- **`PlaylistWidget`** (`components/playlist/playlist.cpp:60-168`):
  - Left: `leftSplitter` holding `PLSelector` and `artContainer` (a `QStackedWidget` with `CoverArtLabel`, :62-82).
  - Right: `StandardPLPanel` (:97).
  - Top bar: `LocationBar`, a view switch and `SearchLineEdit` (:99-128).
- **Separate window:** `PlaylistDialog` (`dialogs/playlist.cpp:36-73`, a `QVLCMW` singleton). `exportPlaylistWidget`/`importPlaylistWidget` move the *same* widget between the window and the main stack (`main_interface.cpp:1033-1067`, `dockPlaylist` :1083).
- **Model:** `PLModel : VLCModel`, one shared instance `PLModel::getPLModel()` = `p_sys->pl_model` (`playlist_model.hpp:50-70`), also used by the popup menu's `QMenuView` (`menus.cpp:1127`). A second view on a different root needs its own `new PLModel(THEPL, p_intf, root, parent)` (`playlist_model.cpp:51`): `rebuild()` re-roots every view of a model.
- **Views** (`views.hpp:85-123`): `PlIconView`, `PlListView`, `PlTreeView`, `PicFlowView`; `showView` / `cycleViews` (`standardpanel.cpp:695, 797`).
- **Core → model:**
  1. Callbacks on `THEPL` (`item-change`, `input-current`, `leaf-to-parent`, `playlist-item-append`, `playlist-item-deleted`, `input_manager.cpp:995-999`) run on core threads.
  2. They `postEvent` to the GUI thread (:1261-1266).
  3. `MainInputManager::customEvent` emits `playlistItemAppended`/`Removed` (:1073-1079), which `PLModel` connects to (`playlist_model.cpp:63-70`).
  4. Core access always holds `PL_LOCK`.
- **Selector** (`PLSelector`, `selector.cpp:224-345`):
  - Nodes: Playlist; Media Library, only if `THEPL->p_media_library` exists (core option `media-library` defaults to 0, `src/libvlc-module.c:2104`); and the category nodes My Computer, Devices, Local Network and Internet, filled from `vlc_sd_GetNames()`.
  - Services-discovery (SD) modules load lazily on first click (`playlist_ServicesDiscoveryAdd`, :358-374), are found again by name (:389, FIXME), and are never unloaded.
- **Search:** `PLModel::filter` → `playlist_LiveSearchUpdate` under the lock, then a rebuild (`standardpanel.cpp:466-482`, `playlist_model.cpp:883-913`). Searchable SDs get `SD_CMD_SEARCH` instead.
- **Context menu:** `StandardPLPanel::popup` (`standardpanel.cpp:157-295`): Play, Pause, Stream, Save, Info, Explore, folders, add, Remove, Clear, Sort by, Shuffle, Display size. Delete/Backspace are hard-coded (:556-557).
- **Drag and drop:** list and tree views accept drops; the icon view only drags (`views.cpp:325-441`). Internal moves go through `PLModel::dropMimeData` (`playlist_model.cpp:156-206`). External drops → `dropEventPlay(event, false, isPL)` (`playlist.cpp:179-187`).
- **Data per item:** title, duration, artist, genre, album, track and disc number, description, URI, rating, cover URL, date (`sorting.h:33-49`, `playlist_model.cpp:306-341`). Roles: `CURRENT_ITEM_ROLE`, `CURRENT_ITEM_CHILD_ROLE`, `LEAF_NODE_ROLE` (`vlc_model.hpp:60-62`).
  - **Art is read synchronously inside paint** (`QImageReader`, `QPixmapCache`, fallback `:/noart`, `vlc_model.cpp:78-109`), which stalls large grids. Art is fetched only with `metadata-network-access` (:199-214).
  - **There are no video thumbnails.**
- **Settings:**
  - QSettings: `Playlist/splitterSizes`, `Playlist/leftSplitterGeometry`, `Playlist/view-mode`, `Playlist/zoom`, `Playlist/headerStateV2`, `playlistdialog` geometry.
  - Config: `playlist-tree`, `qt-opacity`, `metadata-network-access`.

### Recents (`RecentsMRL`, `recents.hpp:54`)
- Two parallel lists saved as `RecentsMRL/list` and `RecentsMRL/times`, capped at 30 (`recents.cpp:140-156`, `recents.hpp:36`). API: `addRecent`, `clear`, `recentList`, `time`, `setTime`, `playMRL`. Reads `qt-recentplay` and `qt-recentplay-filter`; on Windows also calls `SHAddToRecentDocs`.
- **`times` are resume positions in ms**, default `-1`. They're written by `InputManager::delInput` only between 5 % and 95 % of media at least 60 s long (`input_manager.cpp:161-171`), and read back to emit `resumePlayback` (gated by `qt-continue`).
  - **"Continue watching" = recents with `time > 0`, most recent first.**
- `setTime` doesn't save; times are written on the next `addRecent`, on `clear` or in the destructor.

### Menus (`menus.cpp`)
- **`createMenuBar`** (:326-349):
  - Static: Media (:355), Tools (:416), Help (:790).
  - Rebuilt on `aboutToShow` through `menusUpdateMapper` (`BAR_DADD`, :292-296): Playback, Audio, Video, Subtitle, View.
- **Hard-coded shortcuts:**
  - Media: Ctrl+O, Ctrl+Shift+O, Ctrl+F, Ctrl+D, Ctrl+N, Ctrl+C, Ctrl+V (:361-374), Ctrl+Y (:387), Ctrl+R, Ctrl+S (:391-393), Ctrl+Q (:409); recents Ctrl+1–9 (:1622).
  - Tools: Ctrl+E (:419), Ctrl+I (:425), Ctrl+J (:427), Ctrl+Shift+W (:431), Ctrl+M (:438), Ctrl+P (:449).
  - View: Ctrl+L (:491), Ctrl+H (:513), F11 (:523).
  - Others: Ctrl+B (:731), F1 (:794), Ctrl+T (:932), Ctrl+Shift+S (:1107).
  - All other keys go through `key-pressed` → core `key-*`.
- **Popups:** `PopupMenu` (:1013-1155), `VideoPopupMenu`, `AudioPopupMenu`, `MiscPopupMenu` (:965-985), opened through `INTF_DIALOG_*POPUPMENU`. In skins2 dialog-provider mode they add the skins entries (:1096-1119). The systray menu is `updateSystrayMenu` (:1165-1204).
- The builders return plain `QMenu`s, so the menus can be restyled with QSS alone.

### DialogsProvider (`dialogs_provider.cpp`)
- A singleton (`THEDP`), created at `qt.cpp:606` and destroyed at :716. Core requests arrive as `DialogEvent` in `customEvent` (:148-237).
- **Open:** `OpenDialog::getInstance()->showTab(tab)` (:441-476).
- **Preferences:** a new `PrefsDialog` on every open (:250-254).
- **Effects and synchronisation:** `ExtendedDialog` tabs 0 and 2 (:256-276).
- **Media info:** `MediaInfoDialog` (:312-328).
- **Go to time, messages, about, help, bookmarks, plugins:** `toggleVisible()` singletons (:278-355).
- **Toolbar editor:** modal `exec()` (:340-345).
- Only `PlaylistWidget` is built to move between windows today; every other dialog is a top-level `QVLCFrame`/`QVLCDialog`.

### Where the redesign hooks in
- **Playlist panel and library views:** new delegates or grids on `PLModel`. The library uses its own `PLModel` instance on its own root, so it doesn't re-root the docked playlist.
- **Network view:** reuse `vlc_sd_GetNames` and the selector's `setSource` loading.
- **Breakage risks:** the detached playlist window and docking (one movable widget), the View menu's `getPlaylistView()`/`isPlDocked()`, skins2's `PopupMenu`, art loading in paint for big grids.

## 3. Controllers, toolbars, sliders, input, actions

### Toolbar system (`components/controller.*`)
- **`buttonType_e`** (`controller.hpp:56-104`):
  - Buttons 0–25: PLAY, STOP, OPEN, PREV_SLOW, NEXT_FAST, SLOWER, FASTER, FULLSCREEN, DEFULLSCREEN, EXTENDED, PLAYLIST, SNAPSHOT, RECORD, ATOB, FRAME, REVERSE, SKIP_BACK, SKIP_FW, QUIT, RANDOM, LOOP, INFO, PREVIOUS, NEXT, OPEN_SUB, FULLWIDTH; `BUTTON_MAX` = 26.
  - Special widgets 32–44: SPLITTER, INPUT_SLIDER, TIME_LABEL, VOLUME, VOLUME_SPECIAL, MENU_BUTTONS, TELETEXT_BUTTONS, ADVANCED_CONTROLLER, PLAYBACK_BUTTONS, ASPECT_RATIO_COMBOBOX, SPEED_LABEL, TIME_LABEL_ELAPSED, TIME_LABEL_REMAINING; `SPECIAL_MAX` = 45.
  - Spacers 64/65. Flags after `-`: FLAT=1, BIG=2, SHINY=4 (:140-146).
- **Default strings** (`controller.hpp:37-41`): `MAIN_TB1_DEFAULT` `"64;39;64;38;65"`, `MAIN_TB2_DEFAULT`, `ADV_TB_DEFAULT` `"12;11;13;14"`, `INPT_TB_DEFAULT` `"43;33-4;44"`, `FSC_TB_DEFAULT`. They're used only when the settings key is missing, so **existing users keep their saved layouts**.

| Toolbar | Class | Settings key |
|---|---|---|
| Main line 1 | `ControlsWidget` | `MainWindow/MainToolbar1` (:715) |
| Main line 2 | `ControlsWidget` | `MainWindow/MainToolbar2` (:721) |
| Advanced | `AdvControlsWidget` (type 39) | `MainWindow/AdvToolbar` (:760) |
| Input (seek row) | `InputControlsWidget` | `MainWindow/InputToolbar` (:776) |
| Fullscreen controller | `FullscreenControllerWidget` | `MainWindow/FSCtoolbar` (:818), also `FullScreen/pos`, `screen`, `wide` |

- **Parsing:** `parseAndCreate` (:117-163) skips bad tokens with a warning. An empty saved string gives an empty bar; the editor's "Simplest" preset relies on that. `createWidget` (:234-522) is one switch.
- **Wiring:** most buttons use `NORMAL_BUTTON` → `toolbarActionsMapper` → `ActionsManager::doAction`; PREV_SLOW/NEXT_FAST, LOOP and REVERSE are wired directly. Buttons are 26×26 (32 when BIG) with `Qt::NoFocus` (:110-112, :533).
- **Adding a button:**
  1. Insert it before `BUTTON_MAX` (room up to 31).
  2. Add matching entries in `nameL`/`tooltipL`/`iconL` (`controller.hpp:107-138`).
  3. Add an `actionType_e` value, a `doAction` case and a `createWidget` case.
- **Adding a special widget:** insert it before `SPECIAL_MAX` (room up to 63) and add a case in the editor's preview switch (`dialogs/toolbar.cpp:447-580`). **Never renumber**: saved strings store raw numbers. Editor profiles: `ToolbarProfiles/*` with presets hard-coding numbers (`toolbar.hpp:33-44`).

### Seek and volume
- **`SeekSlider`** (`util/input_slider.cpp`) is a `QSlider`; with SHINY it's painted by the `SeekStyle` proxy style (`styles/seekstyle.cpp:67`), which draws chapter ticks and an "Orange" buffering overlay.
  - Wiring: `positionUpdated`→`setPosition`, `cachingChanged`→`updateBuffering`, `inputCanSeek`→`setSeekable`; out `sliderDragged(float)`→`InputManager::sliderUpdate`, throttled to 150 ms.
  - Chapters come from `SeekPoints`. The time tooltip is a top-level `TimeTooltip`.
  - The wheel handler divides by `inputLength` (:195): a 0 length is a divide-by-zero risk.
- **`SoundWidget`/`SoundSlider`** (`components/controller_widget.cpp:50-126`): range `0..qt-max-volume` (default 125, 60–300, `qt.cpp:310`), colours from `qt-slider-colours`. Out via `playlist_VolumeSet`; in via `MainInputManager::volumeChanged`/`soundMuteChanged`.

### InputManager / MainInputManager (`input_manager.*`)
- **`InputManager` signals:** `positionUpdated(float,int64,int)`, `rateChanged`, `nameChanged`, `titleChanged`, `chapterChanged`, `inputCanSeek`, `playingStatusChanged`, `recordingStateChanged`, `cachingChanged`, `artChanged`, `AtoBchanged`, `voutChanged`, `voutListChanged`, teletext, `epgChanged` (:216-255).
- **`MainInputManager` signals:** `inputChanged`, `volumeChanged`, `soundMuteChanged`, `randomChanged`, `repeatLoopChanged`, playlist signals (:333-342).
- **Threads:** core callbacks allocate `IMEvent`/`PLEvent` and `postEvent` to the Qt thread (`input_manager.cpp:318-426, 1211-1285`); `customEvent` emits the signals.
  - `QVLCVariable` emits from the core thread through an auto (queued) connection (`adapters/variables.cpp:48-55`).
  - **Hazard:** `FullscreenControllerWidget::mouseChanged` runs on the vout thread and reads widget state without a lock (`controller.cpp:1314-1334`). Don't copy it.

### Actions and hotkeys
- `actionType_e` (`actions_manager.hpp:38-61`) is separate from `buttonType_e`.
- Keys: `MainInterface::handleKeyPress` → `qtEventToVLCKey` → libvlc `key-pressed` (`main_interface.cpp:1680-1693`), resolved by the core hotkeys module via `key-*`. Qt never reads `key-*`.
- Keys from other windows reach the core through the FSC's `keyPressed` signal and `DialogsProvider` (`dialogs_provider.cpp:870-880`).
- Hard-coded in Qt: the menu accelerators (section 2), plus Shift+L for A–B (`controller.cpp:347`) and Ctrl+K.

## 4. Dialogs, preferences, theming, resources, build

### Options (`qt.cpp:230-322`)
`qt-minimal-view`, `qt-system-tray`, `qt-notification`, `qt-start-minimized`, `qt-pause-minimized`, `qt-opacity`, `qt-fs-opacity`, `qt-video-autoresize`, `qt-name-in-title`, `qt-fs-controller`, `qt-recentplay`, `qt-recentplay-filter`, `qt-continue`, `qt-dark-palette`, `qt-updates-notif`, `qt-updates-days`, `qt-disable-volume-keys`, `qt-embedded-open`, `qt-advanced-pref`, `qt-error-dialogs`, `qt-slider-colours`, `qt-privacy-ask`, `qt-fullscreen-screennumber`, `qt-autoload-extensions`, `qt-bgcone`, `qt-bgcone-expands`, `qt-icon-change`, `qt-max-volume`, `qt-fs-sensitivity`, `qt-auto-raise`.

### Startup order (`ThreadPlatform`)
1. Resources and HiDPI attributes (562-570), `QVLCApp` (573), QSettings (583-598), window icon (600-603).
2. DialogsProvider, MainInputManager (606-607).
3. **MainInterfaceWin32 (632)**.
4. `MainWindow/QtStyle` (681-683), `applyDarkPalette` (686-687), `exec` (690).

The style and palette come *after* the main window exists, and no app-wide stylesheet or font is set. **The theme manager goes before line 632.**
- `isDarkPaletteEnabled` caches in a static (392-395) and is read by `seekstyle.cpp:77`, `input_slider.cpp:637` and `qvlcframe.hpp:205,262,304`.
- `applyDarkPalette` hard-codes colours (397-447) and uses `getWindowsAccentColor` (`qvlcframe.hpp:79-133`).
- The dark title bar calls `winId()` in the `QVLCFrame`/`QVLCDialog`/`QVLCMW` constructors (`qvlcframe.hpp:69-73`), which makes those windows native.

### Hard-coded styling to replace with tokens
- **`setStyleSheet`:** `dialogs/help.cpp:91-96`, `main_interface.cpp:581-586`, `components/interface_widgets.cpp:958`, `components/playlist/selector.cpp:431`, `util/customwidgets.cpp:421`, `util/searchlineedit.cpp:64` (`controller.cpp:707,756,773` only under `DEBUG_LAYOUT`).
- **`.ui` stylesheets:** `about.ui` (16, 105, 140, 261, 296, 309, 322), `open_net.ui:39`, `sprefs_interface.ui:148,321`.
- **`QColor` literals:** `dialogs/plugins.cpp`, `dialogs/toolbar.cpp:356,376`, `styles/seekstyle.cpp`, `util/input_slider.cpp`, `util/pictureflow.cpp`, `util/buttons/RoundButton.cpp`, EPG.
- **Custom `paintEvent`s:** `input_slider.cpp:475,805`, `customwidgets.cpp:49,66,95`, `RoundButton.cpp:72`, `BrowseButton.cpp:59`, `searchlineedit.cpp:132`, `timetooltip.cpp:153`, `pictureflow.cpp:853`, `interface_widgets.cpp:477,627`, `playlist.cpp:368`, `selector.cpp:50`, `toolbar.cpp:313`, EPG.

### Dialogs
- **QVLCDialog singletons:** `ExtendedDialog`, `GotoTimeDialog`, `AboutDialog`, `ErrorsDialog`, `PodcastConfigDialog`.
- **QVLCFrame singletons:** `MediaInfoDialog`, `MessagesDialog`, `HelpDialog`, `UpdateDialog`, `BookmarksDialog`, `PluginDialog`, `EpgDialog`, `VLMDialog`.
- **Not singletons:** `PrefsDialog` (new on each open, `WA_DeleteOnClose`); `OpenDialog` (own `getInstance`, window-modal).
- **Saved geometry:** `EPanel/geometry`, `Preferences`, `Mediainfo`, `Messages`, `Help`, `OpenDialog/size`/`advanced`, `gototimedialog`, `playlistdialog`…
- **Effects** (`components/extended_panels.cpp`): sliders write live aout/vout vars (932, 341-355, 1012) and emit `configChanged`. `ExtendedDialog` collects them per tab (`extended.cpp:185-195`), and **Save** (Audio and Video tabs only) calls `config_Put*` + `config_SaveConfigFile` (197-226). Sync and crop are live only, never saved.
- **Simple Preferences:**
  - `CONFIG_GENERIC`/`CONFIG_BOOL` bind `.ui` widgets to options (`simple_preferences.cpp:289-356`).
  - `apply()` runs the per-panel special cases (1086-1220). `PrefsDialog::save` → `config_SaveConfigFile` → `MainInterface::reloadPrefs` (`preferences.cpp:292-321`).
  - The style combo and the dark checkbox apply immediately and aren't rolled back on Cancel. `simple_preferences.cpp:821` uses a C++14 init-capture that the gnu++11 toolchain already accepts (the full build passed).
- **Open:** panels emit `mrlUpdated(items, options)` (`open_panels.cpp`). `OpenDialog::updateMRL` adds slave, caching, start/stop-time options (`open.cpp:439-457`). `qt-embedded-open` embeds the file picker.
- **Media info:** `MetaPanel`, `ExtraMetaPanel`, `InfoPanel`, `InputStatsPanel`, fed by `InputManager`.
- **Embedding as in-window sheets** loses the `QVLCFrame` Esc/Enter handling (`qvlcframe.hpp:239-295`), the native title bar and saved geometry. `showTab`/`currentTab` must stay for `DialogsProvider`.

### Resources, build, translation
- **Icons:** `vlc.qrc` (130 entries, 112 SVG) referenced as `:/prefix/alias`; `QIcon::fromTheme(name, fallback)` only. SVG works through the statically linked `QSvgIconPlugin`/`QSvgPlugin` (`qt.cpp:60-61`).
- **`Makefile.am`:** sources in `libqt_plugin_la_SOURCES` (48), `*.moc.cpp` in `nodist_libqt_plugin_la_SOURCES` (170), `.ui` in `libqt_plugin_la_UI` (284) with `ui/*.h` in the nodist list (244), resources in `libqt_plugin_la_RES` (318); `vlc.qrc` is compiled by `rcc -name vlc` (457-458).
- **Translation:** `qtr` = `vlc_gettext` (`qt.hpp:129`). `.ui` strings go through `uic -tr "Q_"` (`Makefile.am:308-314`). xgettext keywords: `qtr`, `N_`, `Q_` (`po/Makevars:13`).

## 5. Extension points the redesign uses (summary)
| Need | Hook | Proved by |
|---|---|---|
| Theme (palette + generated QSS + fonts) | New `util/theme.*`, applied in `ThreadPlatform` before `MainInterfaceWin32` is created; replaces `applyDarkPalette` and the static cache | Phase 1 |
| Dark title bar, Mica, live light/dark | One runtime-loaded DWM helper; `WM_SETTINGCHANGE "ImmersiveColorSet"` in the existing native filter | spike c |
| Frameless 40 px title bar with menus | `WindowsCustomMargins` + `WM_NCHITTEST` on the main HWND only; `setMenuWidget()` | spike a |
| Overlays on the video | Top-level `Qt::Tool` windows owned by `MainInterface`, one host tracking `VideoWidget` | spike b |
| Spectrum | `qt_spectrum` audio-filter submodule in `qt.cpp` + bars widget | spike d |
| Decks | New default toolbar strings; append-only `buttonType_e` | section 3 |
| Library | Own `PLModel` per root; recents (`time > 0`) for "continue watching"; SD names for Network | section 2 |
| Playlist panel | Delegate on the shared `PLModel`; the docked/detached widget stays one widget | section 2 |
| Sheets (Effects, Open, Prefs, Info) | Restyle the existing dialogs with tokens first; embed only where `showTab`/Save semantics survive | section 4 |
