/*****************************************************************************
 * redesign_icons.hpp: names of the redesign icon set
 *****************************************************************************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_REDESIGN_ICONS_HPP_
#define VLC_QT_REDESIGN_ICONS_HPP_

/*
 * Every value is the base name of a resource ":/redesign/<name>.svg"
 * (pixmaps/redesign/, Material Symbols Rounded, see LICENSE there).
 * A "_fill" suffix is the filled variant (FILL 1); the rest are outlined.
 * Look icons up by these constants (RedesignIcons::IconPlay), never by
 * string literals.
 *
 * Fill rule of the design: transport buttons, the deck volume button,
 * Record and the library sidebar are filled; everything else, including
 * the same glyphs in menus and context menus, is outlined.
 *
 * Toolbar buttons (buttonType_e, components/controller.hpp iconL):
 *   PLAY_BUTTON          IconPlay / IconPause (playing)
 *   STOP_BUTTON          IconStop
 *   OPEN_BUTTON          IconOpen
 *   PREV_SLOW_BUTTON     IconPrevious
 *   NEXT_FAST_BUTTON     IconNext
 *   SLOWER_BUTTON        IconSlower      (not in the design)
 *   FASTER_BUTTON        IconFaster      (not in the design)
 *   FULLSCREEN_BUTTON    IconFullscreen
 *   DEFULLSCREEN_BUTTON  IconDefullscreen (not in the design)
 *   EXTENDED_BUTTON      IconEffects
 *   PLAYLIST_BUTTON      IconPlaylist
 *   SNAPSHOT_BUTTON      IconSnapshot
 *   RECORD_BUTTON        IconRecord
 *   ATOB_BUTTON          IconAtoB        (the design draws the text "A–B")
 *   FRAME_BUTTON         IconFrame
 *   REVERSE_BUTTON       IconReverse     (not in the design)
 *   SKIP_BACK_BUTTON     IconJumpBackward
 *   SKIP_FW_BUTTON       IconJumpForward
 *   QUIT_BUTTON          IconQuit
 *   RANDOM_BUTTON        IconShuffle
 *   LOOP_BUTTON          IconRepeat / IconRepeatOne
 *   INFO_BUTTON          IconInfo
 *   PREVIOUS_BUTTON      IconPrevious
 *   NEXT_BUTTON          IconNext
 *   OPEN_SUB_BUTTON      IconAddSubtitle
 *   FULLWIDTH_BUTTON     IconDockController (not in the design)
 *   new: tracks          IconTracks
 *   new: mini player     IconMiniPlayer
 *   VOLUME (mute button) IconVolumeOff / IconVolumeDown / IconVolumeUp
 *   ASPECT_RATIO_COMBOBOX IconAspectRatio
 */

/* A namespace, because some names (MenuHelp) are also Win32 functions. */
namespace RedesignIcons
{

/* Transport and deck (filled) */
static const char * const IconPlay           = "play_arrow_fill";
static const char * const IconPause          = "pause_fill";
static const char * const IconStop           = "stop_fill";
static const char * const IconPrevious       = "skip_previous_fill";
static const char * const IconNext           = "skip_next_fill";
static const char * const IconReverse        = "fast_rewind_fill";
static const char * const IconRecord         = "fiber_manual_record_fill";
/* volume < 50 % is "down", 0 or muted is "off" */
static const char * const IconVolumeOff      = "volume_off_fill";
static const char * const IconVolumeDown     = "volume_down_fill";
static const char * const IconVolumeUp       = "volume_up_fill";
static const char * const IconNowPlaying     = "graphic_eq_fill";

/* Deck and toolbar (outlined) */
static const char * const IconRepeat         = "repeat";
static const char * const IconRepeatOne      = "repeat_one";
static const char * const IconShuffle        = "shuffle";
static const char * const IconTracks         = "subtitles";
static const char * const IconEffects        = "tune";
static const char * const IconPlaylist       = "playlist_play";
static const char * const IconMiniPlayer     = "picture_in_picture_alt";
static const char * const IconFullscreen     = "fullscreen";
static const char * const IconDefullscreen   = "fullscreen_exit";
static const char * const IconExpand         = "open_in_full";
static const char * const IconFrame          = "keyboard_tab";
static const char * const IconSnapshot       = "photo_camera";
static const char * const IconAspectRatio    = "aspect_ratio";
static const char * const IconAtoB           = "laps";
static const char * const IconSlower         = "slow_motion_video";
static const char * const IconFaster         = "speed";
static const char * const IconJumpBackward   = "replay_10";
static const char * const IconJumpForward    = "forward_10";
static const char * const IconReplay         = "replay";
static const char * const IconCast           = "cast";
static const char * const IconOpen           = "file_open";
static const char * const IconAddSubtitle    = "note_add";
static const char * const IconQuit           = "logout";
static const char * const IconDockController = "dock_to_bottom";

/* Title bar */
static const char * const IconCone           = "cone"; /* full colour, not tinted */
static const char * const IconMinimize       = "remove";
static const char * const IconMaximize       = "crop_square";
static const char * const IconClose          = "close";

/* Library sidebar (filled) and library content */
static const char * const IconNavHome        = "home_fill";
static const char * const IconNavVideos      = "movie_fill";
static const char * const IconNavMusic       = "library_music_fill";
static const char * const IconNavNetwork     = "lan_fill";
static const char * const IconOpenMedia      = "add";
static const char * const IconSettings       = "settings";
static const char * const IconSearch         = "search";
static const char * const IconBack           = "arrow_back";
static const char * const IconFolder         = "folder";
static const char * const IconAddFolder      = "create_new_folder";
static const char * const IconHistory        = "history";
static const char * const IconVideo          = "movie";
static const char * const IconMusic          = "music_note";
static const char * const IconStream         = "podcasts";
static const char * const IconComputer       = "computer";
static const char * const IconServer         = "dns";
static const char * const IconRadio          = "radio";
static const char * const IconPodcast        = "rss_feed";

/* Preferences sections */
static const char * const IconPrefInterface  = "web";
static const char * const IconPrefAudio      = "graphic_eq";
static const char * const IconPrefVideo      = "movie";
static const char * const IconPrefSubtitles  = "subtitles";
static const char * const IconPrefInput      = "input";
static const char * const IconPrefHotkeys    = "keyboard";
static const char * const IconPrefAdvanced   = "code";

/* Open dialog tabs */
static const char * const IconOpenFile       = "draft";
static const char * const IconOpenDisc       = "album";
static const char * const IconOpenNetwork    = "language";
static const char * const IconOpenCapture    = "videocam";
static const char * const IconUploadFile     = "upload_file";

/* Dialogs, controls and states */
static const char * const IconAdd            = "add";
static const char * const IconRemove         = "remove";
static const char * const IconCheck          = "check";
static const char * const IconUnchecked      = "check_box_outline_blank";
static const char * const IconExpandMore     = "expand_more";
static const char * const IconSubmenu        = "chevron_right";
static const char * const IconInfo           = "info";
static const char * const IconError          = "error";

/* Playlist context menu (outlined; Play and Shuffle reuse the menu names) */
static const char * const IconSave           = "save";
static const char * const IconShowFolder     = "folder_open";
static const char * const IconCopy           = "content_copy";
static const char * const IconDelete         = "delete";
static const char * const IconSort           = "sort";

/* Menu bar items (outlined), named after the menu entry */
static const char * const MenuOpenFile       = "draft";
static const char * const MenuOpenFiles      = "file_copy";
static const char * const MenuOpenFolder     = "folder_open";
static const char * const MenuOpenDisc       = "album";
static const char * const MenuOpenNetwork    = "language";
static const char * const MenuOpenCapture    = "videocam";
static const char * const MenuOpenRecent     = "history";
static const char * const MenuSavePlaylist   = "playlist_add_check";
static const char * const MenuConvert        = "sync_alt";
static const char * const MenuStream         = "cast";
static const char * const MenuQuitAtEnd      = "playlist_remove";
static const char * const MenuQuit           = "logout";
static const char * const MenuTitle          = "title";
static const char * const MenuChapter        = "bookmarks";
static const char * const MenuSpeed          = "speed";
static const char * const MenuJumpForward    = "forward_10";
static const char * const MenuJumpBackward   = "replay_10";
static const char * const MenuJumpToTime     = "schedule";
static const char * const MenuPlay           = "play_arrow";
static const char * const MenuPause          = "pause";
static const char * const MenuStop           = "stop";
static const char * const MenuPrevious       = "skip_previous";
static const char * const MenuNext           = "skip_next";
static const char * const MenuRecord         = "fiber_manual_record";
/* the design's "audiotrack" ligature is the same glyph as music_note */
static const char * const MenuAudioTrack     = "music_note";
static const char * const MenuAudioDevice    = "speaker";
static const char * const MenuStereoMode     = "surround_sound";
static const char * const MenuVisualizations = "equalizer";
static const char * const MenuAudioMode      = "library_music";
static const char * const MenuVolumeUp       = "volume_up";
static const char * const MenuVolumeDown     = "volume_down";
static const char * const MenuMute           = "volume_off";
static const char * const MenuVideoTrack     = "movie";
static const char * const MenuFullscreen     = "fullscreen";
static const char * const MenuFitWindow      = "fit_screen";
static const char * const MenuWallpaper      = "wallpaper";
static const char * const MenuZoom           = "zoom_in";
static const char * const MenuAspectRatio    = "aspect_ratio";
static const char * const MenuCrop           = "crop";
static const char * const MenuDeinterlace    = "blur_linear";
static const char * const MenuSnapshot       = "photo_camera";
static const char * const MenuAddSubtitle    = "note_add";
static const char * const MenuSubtitleTrack  = "subtitles";
static const char * const MenuTrackSync      = "av_timer";
static const char * const MenuEffects        = "tune";
static const char * const MenuMediaInfo      = "info";
static const char * const MenuCodecInfo      = "memory";
static const char * const MenuVLM            = "dns";
static const char * const MenuProgramGuide   = "event_note";
static const char * const MenuMessages       = "terminal";
static const char * const MenuExtensions     = "extension";
static const char * const MenuPreferences    = "settings";
static const char * const MenuMediaLibrary   = "video_library";
static const char * const MenuPlaylist       = "playlist_play";
static const char * const MenuMenuBar        = "menu";
static const char * const MenuMiniPlayer     = "picture_in_picture_alt";
static const char * const MenuFullscreenUI   = "fullscreen";
static const char * const MenuAdvControls    = "construction";
static const char * const MenuStatusBar      = "call_to_action";
static const char * const MenuHelp           = "help";
static const char * const MenuUpdates        = "update";
static const char * const MenuAbout          = "info";
/* entries the design's menus leave out (3.0 has more than the prototype) */
static const char * const MenuOpenClipboard  = "content_paste";
static const char * const MenuCloseToSystray = "minimize";
static const char * const MenuProgram        = "live_tv";
static const char * const MenuBookmarks      = "bookmark";
static const char * const MenuBookmarksManage = "edit";
static const char * const MenuPrevTitle      = "first_page";
static const char * const MenuNextTitle      = "last_page";
static const char * const MenuPrevChapter    = "chevron_left";
static const char * const MenuNextChapter    = "chevron_right";
static const char * const MenuNormalSpeed    = "1x_mobiledata";
static const char * const MenuRenderer       = "cast";
static const char * const MenuLeaveFullscreen = "fullscreen_exit";
static const char * const MenuDeinterlaceMode = "blur_on";
static const char * const MenuDockedPlaylist = "dock_to_right";
static const char * const MenuPlaylistView   = "view_list";
static const char * const MenuAlwaysOnTop    = "keep";
static const char * const MenuImmersive      = "open_in_full";
static const char * const MenuInterfaces     = "web";
static const char * const MenuCustomize      = "dashboard_customize";
static const char * const MenuClearRecent    = "delete";
static const char * const MenuOpenMedia      = "file_open";
/* the right-click menu's submenus */
static const char * const MenuAudio          = "graphic_eq";
static const char * const MenuVideo          = "movie";
static const char * const MenuSubtitle       = "subtitles";
static const char * const MenuPlayback       = "play_arrow";
static const char * const MenuTools          = "build";
static const char * const MenuView           = "visibility";

/* Effects sheet, "More" menu */
static const char * const EffectCompressor   = "compress";
static const char * const EffectSpatializer  = "spatial_audio";
static const char * const EffectWidener      = "width";
static const char * const EffectCrop         = "crop";
static const char * const EffectColors       = "palette";
static const char * const EffectGeometry     = "transform";
static const char * const EffectOverlay      = "layers";
static const char * const EffectAdvanced     = "code";

}

#endif
