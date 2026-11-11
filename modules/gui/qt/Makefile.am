## Howto
# For each Q_OBJECT:
#   - Add *.moc.cpp to nodist_..._SOURCES
#   - Add *.cpp to ..._SOURCES
#   - Add *.hpp to ..._SOURCES
# For each UI
#   - Add *.ui to ..._UI
#   - Add *.h to nodist_..._SOURCES
# For each ressource (icon, png, ...)
#   - Add it to DEPS_res

SUFFIXES += .ui .h .hpp .moc.cpp
EXTRA_DIST += gui/qt/qt6.pro gui/qt/private-gui.pro

libqt_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DQT_USE_QSTRINGBUILDER \
	-DQT_NO_CAST_FROM_BYTEARRAY \
	-I$(srcdir)/gui/qt -I$(builddir)/gui/qt
libqt_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) $(CXXFLAGS_qt)
libqt_plugin_la_LIBADD = $(QT_LIBS) $(LIBS_qt) $(LIBM)
libqt_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)'
if HAVE_DARWIN
libqt_plugin_la_LDFLAGS += -Wl,-framework,Cocoa
endif
if HAVE_QT_X11
libqt_plugin_la_CXXFLAGS += -DQT_HAS_X11
libqt_plugin_la_LIBADD += $(X_LIBS) $(X_PRE_LIBS) -lX11
endif
if HAVE_QT5_X11
# Qt5 needs Qt5X11Extras for QX11Info
libqt_plugin_la_CXXFLAGS += $(QT5_X11_CFLAGS)
libqt_plugin_la_LIBADD += $(QT5_X11_LIBS)
endif
if HAVE_XCB
libqt_plugin_la_CXXFLAGS += -DQT_HAS_XCB
endif
if HAVE_WIN32
libqt_plugin_la_LIBADD += $(LIBCOM) -lcomctl32 -luuid
# a private QPA header (main_interface_win32.cpp), in the include tree of
# the Qt version the build found
libqt_plugin_la_CPPFLAGS += '-DVLC_QT_WSI_H=<QtGui/$(QT_VERSION)/QtGui/qpa/qwindowsysteminterface.h>'
endif
if UPDATE_CHECK
libqt_plugin_la_CPPFLAGS += -DUPDATE_CHECK
endif

if HAVE_QT_GUI_PRIVATE
libqt_plugin_la_CPPFLAGS += -DQT_GUI_PRIVATE $(QT_GUI_PRIVATE_CFLAGS)
endif

libqt_plugin_la_SOURCES = \
	gui/qt/qt.cpp gui/qt/qt.hpp gui/qt/qt_x11.hpp \
	gui/qt/menus.cpp gui/qt/menus.hpp \
	gui/qt/main_interface.cpp gui/qt/main_interface.hpp \
	gui/qt/dialogs_provider.cpp gui/qt/dialogs_provider.hpp \
	gui/qt/input_manager.cpp gui/qt/input_manager.hpp \
	gui/qt/actions_manager.cpp gui/qt/actions_manager.hpp \
	gui/qt/extensions_manager.cpp gui/qt/extensions_manager.hpp \
	gui/qt/managers/addons_manager.cpp gui/qt/managers/addons_manager.hpp \
	gui/qt/recents.cpp gui/qt/recents.hpp \
	gui/qt/adapters/seekpoints.cpp gui/qt/adapters/seekpoints.hpp \
	gui/qt/adapters/chromaprint.cpp gui/qt/adapters/chromaprint.hpp \
	gui/qt/adapters/variables.cpp gui/qt/adapters/variables.hpp \
	gui/qt/dialogs/playlist.cpp gui/qt/dialogs/playlist.hpp \
	gui/qt/dialogs/bookmarks.cpp gui/qt/dialogs/bookmarks.hpp \
	gui/qt/dialogs/preferences.cpp gui/qt/dialogs/preferences.hpp \
	gui/qt/dialogs/mediainfo.cpp gui/qt/dialogs/mediainfo.hpp \
	gui/qt/dialogs/epg.cpp gui/qt/dialogs/epg.hpp \
	gui/qt/dialogs/extended.cpp gui/qt/dialogs/extended.hpp \
	gui/qt/dialogs/messages.cpp gui/qt/dialogs/messages.hpp \
	gui/qt/dialogs/errors.cpp gui/qt/dialogs/errors.hpp \
	gui/qt/dialogs/external.cpp gui/qt/dialogs/external.hpp \
	gui/qt/dialogs/plugins.cpp gui/qt/dialogs/plugins.hpp \
	gui/qt/dialogs/sout.cpp gui/qt/dialogs/sout.hpp \
	gui/qt/dialogs/convert.cpp gui/qt/dialogs/convert.hpp \
	gui/qt/dialogs/help.cpp gui/qt/dialogs/help.hpp \
	gui/qt/dialogs/gototime.cpp gui/qt/dialogs/gototime.hpp \
	gui/qt/dialogs/toolbar.cpp gui/qt/dialogs/toolbar.hpp \
	gui/qt/dialogs/open.cpp gui/qt/dialogs/open.hpp \
	gui/qt/dialogs/openurl.cpp gui/qt/dialogs/openurl.hpp \
	gui/qt/dialogs/vlm.cpp gui/qt/dialogs/vlm.hpp \
	gui/qt/dialogs/firstrun.cpp gui/qt/dialogs/firstrun.hpp \
	gui/qt/dialogs/podcast_configuration.cpp \
	gui/qt/dialogs/podcast_configuration.hpp \
	gui/qt/dialogs/extensions.cpp gui/qt/dialogs/extensions.hpp \
	gui/qt/dialogs/fingerprintdialog.cpp \
	gui/qt/dialogs/fingerprintdialog.hpp \
	gui/qt/components/extended_panels.cpp \
	gui/qt/components/extended_panels.hpp \
	gui/qt/components/info_panels.cpp gui/qt/components/info_panels.hpp \
	gui/qt/components/info_widgets.cpp gui/qt/components/info_widgets.hpp \
	gui/qt/components/preferences_widgets.cpp \
	gui/qt/components/preferences_widgets.hpp \
	gui/qt/components/complete_preferences.cpp \
	gui/qt/components/complete_preferences.hpp \
	gui/qt/components/simple_preferences.cpp \
	gui/qt/components/simple_preferences.hpp \
	gui/qt/components/open_panels.cpp gui/qt/components/open_panels.hpp \
	gui/qt/components/interface_widgets.cpp \
	gui/qt/components/interface_widgets.hpp \
	gui/qt/components/controller.cpp gui/qt/components/controller.hpp \
	gui/qt/components/deck_popover.cpp gui/qt/components/deck_popover.hpp \
	gui/qt/components/overlay_host.cpp gui/qt/components/overlay_host.hpp \
	gui/qt/components/toast.cpp gui/qt/components/toast.hpp \
	gui/qt/components/drop_overlay.cpp gui/qt/components/drop_overlay.hpp \
	gui/qt/components/video_overlays.cpp gui/qt/components/video_overlays.hpp \
	gui/qt/components/sidebar.cpp gui/qt/components/sidebar.hpp \
	gui/qt/components/prefs_page.cpp gui/qt/components/prefs_page.hpp \
	gui/qt/components/title_bar.cpp gui/qt/components/title_bar.hpp \
	gui/qt/components/now_playing.cpp gui/qt/components/now_playing.hpp \
	gui/qt/components/library/library_widgets.cpp gui/qt/components/library/library_widgets.hpp \
	gui/qt/components/library/media_page.cpp gui/qt/components/library/media_page.hpp \
	gui/qt/components/library/network_page.cpp gui/qt/components/library/network_page.hpp \
	gui/qt/components/library/thumbnailer.cpp gui/qt/components/library/thumbnailer.hpp \
	gui/qt/components/spectrum.cpp gui/qt/components/spectrum.hpp \
	gui/qt/components/library/home_page.cpp \
	gui/qt/components/library/home_page.hpp \
	gui/qt/components/controller_widget.cpp \
	gui/qt/components/controller_widget.hpp \
	gui/qt/components/epg/EPGChannels.cpp \
	gui/qt/components/epg/EPGChannels.hpp \
	gui/qt/components/epg/EPGItem.cpp \
	gui/qt/components/epg/EPGItem.hpp \
	gui/qt/components/epg/EPGProgram.cpp \
	gui/qt/components/epg/EPGProgram.hpp \
	gui/qt/components/epg/EPGRuler.cpp gui/qt/components/epg/EPGRuler.hpp \
	gui/qt/components/epg/EPGView.cpp gui/qt/components/epg/EPGView.hpp \
	gui/qt/components/epg/EPGWidget.cpp \
	gui/qt/components/epg/EPGWidget.hpp \
	gui/qt/components/playlist/views.cpp \
	gui/qt/components/playlist/views.hpp \
	gui/qt/components/playlist/vlc_model.cpp \
	gui/qt/components/playlist/vlc_model.hpp \
	gui/qt/components/playlist/playlist_model.cpp \
	gui/qt/components/playlist/playlist_model.hpp \
	gui/qt/components/playlist/playlist_item.cpp \
	gui/qt/components/playlist/playlist_item.hpp \
	gui/qt/components/playlist/standardpanel.cpp \
	gui/qt/components/playlist/standardpanel.hpp \
	gui/qt/components/playlist/playlist_panel.cpp \
	gui/qt/components/playlist/playlist_panel.hpp \
	gui/qt/components/playlist/playlist.cpp \
	gui/qt/components/playlist/playlist.hpp \
	gui/qt/components/playlist/selector.cpp \
	gui/qt/components/playlist/selector.hpp \
	gui/qt/components/playlist/sorting.h \
	gui/qt/components/sout/profile_selector.cpp \
	gui/qt/components/sout/profile_selector.hpp \
	gui/qt/components/sout/sout_widgets.cpp \
	gui/qt/components/sout/sout_widgets.hpp \
	gui/qt/components/sout/profiles.hpp \
	gui/qt/util/animators.cpp gui/qt/util/animators.hpp \
	gui/qt/util/input_slider.cpp gui/qt/util/input_slider.hpp \
	gui/qt/util/timetooltip.cpp gui/qt/util/timetooltip.hpp \
	gui/qt/util/customwidgets.cpp gui/qt/util/customwidgets.hpp \
	gui/qt/util/searchlineedit.cpp gui/qt/util/searchlineedit.hpp \
	gui/qt/util/registry.cpp gui/qt/util/registry.hpp \
	gui/qt/util/qmenuview.cpp gui/qt/util/qmenuview.hpp \
	gui/qt/util/qt_dirs.cpp gui/qt/util/qt_dirs.hpp \
	gui/qt/util/pictureflow.cpp gui/qt/util/pictureflow.hpp \
	gui/qt/util/validators.cpp gui/qt/util/validators.hpp \
	gui/qt/util/redesign_icons.hpp \
	gui/qt/util/spectrum_analysis.hpp \
	gui/qt/util/buttons/BrowseButton.cpp \
	gui/qt/util/buttons/BrowseButton.hpp \
	gui/qt/util/buttons/DeckButtonsLayout.cpp \
	gui/qt/util/buttons/DeckButtonsLayout.hpp \
	gui/qt/util/buttons/RoundButton.cpp \
	gui/qt/util/buttons/RoundButton.hpp \
	gui/qt/util/qvlcframe.hpp \
	gui/qt/util/qvlcapp.hpp \
	gui/qt/util/singleton.hpp \
	gui/qt/util/imagehelper.cpp gui/qt/util/imagehelper.hpp \
	gui/qt/util/theme.cpp gui/qt/util/theme.hpp gui/qt/util/theme_tokens.hpp \
	gui/qt/util/breakpoints.cpp gui/qt/util/breakpoints.hpp \
	gui/qt/styles/seekstyle.cpp gui/qt/styles/seekstyle.hpp
if HAVE_WIN32
libqt_plugin_la_SOURCES += \
	gui/qt/main_interface_win32.cpp gui/qt/main_interface_win32.hpp \
	gui/qt/util/win_theme.hpp
endif

# Meta-object compilation

moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose__$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC   " $@;
moc_verbose__0 = $(moc_verbose_0)

.hpp.moc.cpp:
	$(moc_verbose)$(MOC) $(MOC_CPPFLAGS) -o $@ $<

# These MOC files depend on the configure settings:
dialogs_provider.moc.cpp dialogs/vlm.moc.cpp: $(top_builddir)/config.h

nodist_libqt_plugin_la_SOURCES = \
	gui/qt/main_interface.moc.cpp \
	gui/qt/menus.moc.cpp \
	gui/qt/dialogs_provider.moc.cpp \
	gui/qt/input_manager.moc.cpp \
	gui/qt/actions_manager.moc.cpp \
	gui/qt/extensions_manager.moc.cpp \
	gui/qt/managers/addons_manager.moc.cpp \
	gui/qt/recents.moc.cpp \
	gui/qt/adapters/seekpoints.moc.cpp \
	gui/qt/adapters/chromaprint.moc.cpp \
	gui/qt/adapters/variables.moc.cpp \
	gui/qt/dialogs/playlist.moc.cpp \
	gui/qt/dialogs/bookmarks.moc.cpp \
	gui/qt/dialogs/mediainfo.moc.cpp \
	gui/qt/dialogs/extended.moc.cpp \
	gui/qt/dialogs/messages.moc.cpp \
	gui/qt/dialogs/epg.moc.cpp \
	gui/qt/dialogs/errors.moc.cpp \
	gui/qt/dialogs/external.moc.cpp \
	gui/qt/dialogs/plugins.moc.cpp \
	gui/qt/dialogs/preferences.moc.cpp \
	gui/qt/dialogs/sout.moc.cpp \
	gui/qt/dialogs/convert.moc.cpp \
	gui/qt/dialogs/help.moc.cpp \
	gui/qt/dialogs/gototime.moc.cpp \
	gui/qt/dialogs/toolbar.moc.cpp \
	gui/qt/dialogs/open.moc.cpp \
	gui/qt/dialogs/openurl.moc.cpp \
	gui/qt/dialogs/podcast_configuration.moc.cpp \
	gui/qt/dialogs/firstrun.moc.cpp \
	gui/qt/dialogs/extensions.moc.cpp \
	gui/qt/dialogs/fingerprintdialog.moc.cpp \
	gui/qt/components/extended_panels.moc.cpp \
	gui/qt/components/info_panels.moc.cpp \
	gui/qt/components/info_widgets.moc.cpp \
	gui/qt/components/preferences_widgets.moc.cpp \
	gui/qt/components/complete_preferences.moc.cpp \
	gui/qt/components/simple_preferences.moc.cpp \
	gui/qt/components/open_panels.moc.cpp \
	gui/qt/components/interface_widgets.moc.cpp \
	gui/qt/components/controller.moc.cpp \
	gui/qt/components/overlay_host.moc.cpp \
	gui/qt/components/toast.moc.cpp \
	gui/qt/components/sidebar.moc.cpp \
	gui/qt/components/prefs_page.moc.cpp \
	gui/qt/components/title_bar.moc.cpp \
	gui/qt/components/now_playing.moc.cpp \
	gui/qt/components/library/library_widgets.moc.cpp \
	gui/qt/components/library/media_page.moc.cpp \
	gui/qt/components/library/network_page.moc.cpp \
	gui/qt/components/library/thumbnailer.moc.cpp \
	gui/qt/components/spectrum.moc.cpp \
	gui/qt/components/library/home_page.moc.cpp \
	gui/qt/components/controller_widget.moc.cpp \
	gui/qt/components/epg/EPGChannels.moc.cpp \
	gui/qt/components/epg/EPGProgram.moc.cpp \
	gui/qt/components/epg/EPGRuler.moc.cpp \
	gui/qt/components/epg/EPGView.moc.cpp \
	gui/qt/components/epg/EPGWidget.moc.cpp \
	gui/qt/components/playlist/views.moc.cpp \
	gui/qt/components/playlist/vlc_model.moc.cpp \
	gui/qt/components/playlist/playlist_model.moc.cpp \
	gui/qt/components/playlist/playlist.moc.cpp \
	gui/qt/components/playlist/standardpanel.moc.cpp \
	gui/qt/components/playlist/playlist_panel.moc.cpp \
	gui/qt/components/playlist/selector.moc.cpp \
	gui/qt/components/sout/profile_selector.moc.cpp \
	gui/qt/components/sout/sout_widgets.moc.cpp \
	gui/qt/util/animators.moc.cpp \
	gui/qt/util/input_slider.moc.cpp \
	gui/qt/util/timetooltip.moc.cpp \
	gui/qt/util/customwidgets.moc.cpp \
	gui/qt/util/searchlineedit.moc.cpp \
	gui/qt/util/qmenuview.moc.cpp \
	gui/qt/util/qvlcapp.moc.cpp \
	gui/qt/util/theme.moc.cpp \
	gui/qt/util/breakpoints.moc.cpp \
	gui/qt/util/pictureflow.moc.cpp \
	gui/qt/util/validators.moc.cpp \
	gui/qt/util/buttons/RoundButton.moc.cpp \
	gui/qt/util/buttons/DeckButtonsLayout.moc.cpp \
	gui/qt/util/buttons/BrowseButton.moc.cpp \
	gui/qt/styles/seekstyle.moc.cpp

if HAVE_WIN32
nodist_libqt_plugin_la_SOURCES += gui/qt/main_interface_win32.moc.cpp
endif

nodist_libqt_plugin_la_SOURCES += \
	gui/qt/ui/equalizer.h \
	gui/qt/ui/video_effects.h \
	gui/qt/ui/open_file.h \
	gui/qt/ui/open_disk.h \
	gui/qt/ui/open_net.h \
	gui/qt/ui/open_capture.h \
	gui/qt/ui/open.h \
	gui/qt/ui/vlm.h \
	gui/qt/ui/podcast_configuration.h \
	gui/qt/ui/profiles.h \
	gui/qt/ui/sprefs_audio.h \
	gui/qt/ui/sprefs_input.h \
	gui/qt/ui/sprefs_interface.h \
	gui/qt/ui/sprefs_subtitles.h \
	gui/qt/ui/sprefs_video.h \
	gui/qt/ui/streampanel.h \
	gui/qt/ui/messages_panel.h \
	gui/qt/ui/about.h \
	gui/qt/ui/update.h \
	gui/qt/ui/fingerprintdialog.h \
	gui/qt/ui/sout.h

# User interface compilation

uic_verbose = $(uic_verbose_$(V))
uic_verbose_ = $(uic_verbose__$(AM_DEFAULT_VERBOSITY))
uic_verbose_0 = @echo "  UIC   " $@;
uic_verbose__0 = $(uic_verbose_0)

MOC_CPPFLAGS = $(DEFS) \
	-I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_builddir) \
	$(libqt_plugin_la_CPPFLAGS)
if HAVE_DARWIN
MOC_CPPFLAGS += -DQ_OS_MAC
endif
if HAVE_WIN32
MOC_CPPFLAGS += -D_WIN32
endif

libqt_plugin_la_UI = \
	gui/qt/ui/equalizer.ui \
	gui/qt/ui/video_effects.ui \
	gui/qt/ui/open_file.ui \
	gui/qt/ui/open_disk.ui \
	gui/qt/ui/open_net.ui \
	gui/qt/ui/open_capture.ui \
	gui/qt/ui/open.ui \
	gui/qt/ui/podcast_configuration.ui \
	gui/qt/ui/profiles.ui \
	gui/qt/ui/sprefs_audio.ui \
	gui/qt/ui/sprefs_input.ui \
	gui/qt/ui/sprefs_interface.ui \
	gui/qt/ui/sprefs_subtitles.ui \
	gui/qt/ui/sprefs_video.ui \
	gui/qt/ui/streampanel.ui \
	gui/qt/ui/messages_panel.ui \
	gui/qt/ui/about.ui \
	gui/qt/ui/update.ui \
	gui/qt/ui/sout.ui \
	gui/qt/ui/vlm.ui \
	gui/qt/ui/fingerprintdialog.ui
EXTRA_DIST += $(libqt_plugin_la_UI)

.ui.h:
	$(AM_V_at)mkdir -p -- gui/qt/ui
	$(AM_V_at)rm -f $@ $@.tmp
	$(AM_V_at)echo "#define Q_(a,b) QString::fromUtf8(_(a))" > $@.tmp
	$(uic_verbose)$(UIC) -tr "Q_" $< >> $@.tmp
	$(AM_V_at)sed -e 's/Q_(\"_(\\\"\(.*\)\\\")"/Q_("\1"/' $@.tmp >$@
	$(AM_V_at)rm -f $@.tmp

# Resources

libqt_plugin_la_RES = \
	gui/qt/pixmaps/clear.svg \
	gui/qt/pixmaps/eject.svg \
	gui/qt/pixmaps/faster.svg \
	gui/qt/pixmaps/faster2.svg \
	gui/qt/pixmaps/go-next.svg \
	gui/qt/pixmaps/addons/addon.svg \
	gui/qt/pixmaps/addons/addon_broken.svg \
	gui/qt/pixmaps/addons/addon_blue.svg \
	gui/qt/pixmaps/addons/addon_cyan.svg \
	gui/qt/pixmaps/addons/addon_green.svg \
	gui/qt/pixmaps/addons/addon_magenta.svg \
	gui/qt/pixmaps/addons/addon_red.svg \
	gui/qt/pixmaps/addons/addon_yellow.svg \
	gui/qt/pixmaps/addons/score.svg \
	gui/qt/pixmaps/menus/exit_16px.svg \
	gui/qt/pixmaps/menus/help_16px.svg \
	gui/qt/pixmaps/menus/info_16px.svg \
	gui/qt/pixmaps/menus/messages_16px.svg \
	gui/qt/pixmaps/menus/playlist_16px.svg \
	gui/qt/pixmaps/menus/preferences_16px.svg \
	gui/qt/pixmaps/menus/settings_16px.svg \
	gui/qt/pixmaps/menus/stream_16px.svg \
	gui/qt/pixmaps/next.svg \
	gui/qt/pixmaps/noart-64.png \
	gui/qt/pixmaps/noart.png \
	gui/qt/pixmaps/pause.svg \
	gui/qt/pixmaps/play.svg \
	gui/qt/pixmaps/playlist/add.svg \
	gui/qt/pixmaps/playlist/playlist.svg \
	gui/qt/pixmaps/playlist/remove.svg \
	gui/qt/pixmaps/playlist/repeat_all.svg \
	gui/qt/pixmaps/playlist/repeat_off.svg \
	gui/qt/pixmaps/playlist/repeat_one.svg \
	gui/qt/pixmaps/playlist/shuffle_on.svg \
	gui/qt/pixmaps/playlist/dropzone.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/capture.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/pictures.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/disc.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/playlist.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/lan.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/podcast.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/library.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/screen.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/movie.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/mtp.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/music.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/network.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/appletrailers.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/icecast.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/katsomo.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/fmc.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/frenchtv.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/jamendo.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/assembleenationale.svg \
	gui/qt/pixmaps/prefs/advprefs_audio.svg \
	gui/qt/pixmaps/prefs/advprefs_codec.svg \
	gui/qt/pixmaps/prefs/advprefs_extended.svg \
	gui/qt/pixmaps/prefs/advprefs_intf.svg \
	gui/qt/pixmaps/prefs/advprefs_playlist.svg \
	gui/qt/pixmaps/prefs/advprefs_sout.svg \
	gui/qt/pixmaps/prefs/advprefs_video.svg \
	gui/qt/pixmaps/prefs/spref_cone_Audio_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Hotkeys_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Input_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Interface_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Subtitles_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Video_64.png \
	gui/qt/pixmaps/previous.svg \
	gui/qt/pixmaps/profile_new.svg \
	gui/qt/pixmaps/sample_complete.png \
	gui/qt/pixmaps/sample_minimal.png \
	gui/qt/pixmaps/sample_skins.png \
	gui/qt/pixmaps/slower.svg \
	gui/qt/pixmaps/slower2.svg \
	gui/qt/pixmaps/space.svg \
	gui/qt/pixmaps/stop.svg \
	gui/qt/pixmaps/toolbar/arrows.svg \
	gui/qt/pixmaps/toolbar/aspect-ratio.svg \
	gui/qt/pixmaps/toolbar/atob.svg \
	gui/qt/pixmaps/toolbar/atob_noa.svg \
	gui/qt/pixmaps/toolbar/atob_nob.svg \
	gui/qt/pixmaps/toolbar/renderer.svg \
	gui/qt/pixmaps/toolbar/defullscreen.svg \
	gui/qt/pixmaps/toolbar/dvd_menu.svg \
	gui/qt/pixmaps/toolbar/dvd_next.svg \
	gui/qt/pixmaps/toolbar/dvd_prev.svg \
	gui/qt/pixmaps/toolbar/extended_16px.svg \
	gui/qt/pixmaps/toolbar/frame-by-frame.svg \
	gui/qt/pixmaps/toolbar/fullscreen.svg \
	gui/qt/pixmaps/toolbar/play_reverse.svg \
	gui/qt/pixmaps/toolbar/record_16px.svg \
	gui/qt/pixmaps/toolbar/snapshot.svg \
	gui/qt/pixmaps/toolbar/skip_for.svg \
	gui/qt/pixmaps/toolbar/skip_back.svg \
	gui/qt/pixmaps/toolbar/tv.svg \
	gui/qt/pixmaps/toolbar/tvtelx.svg \
	gui/qt/pixmaps/toolbar/visu.svg \
	gui/qt/pixmaps/toolbar/volume-high.svg \
	gui/qt/pixmaps/toolbar/volume-low.svg \
	gui/qt/pixmaps/toolbar/volume-medium.svg \
	gui/qt/pixmaps/toolbar/volume-muted.svg \
	gui/qt/pixmaps/toolbar/volume-slider-inside.svg \
	gui/qt/pixmaps/toolbar/volume-slider-outside.svg \
	gui/qt/pixmaps/types/capture-card_16px.svg \
	gui/qt/pixmaps/types/disc_16px.svg \
	gui/qt/pixmaps/types/file-asym_16px.svg \
	gui/qt/pixmaps/types/file-wide_16px.svg \
	gui/qt/pixmaps/types/folder-blue_16px.svg \
	gui/qt/pixmaps/types/folder-grey_16px.svg \
	gui/qt/pixmaps/types/harddisk_16px.svg \
	gui/qt/pixmaps/types/network_16px.svg \
	gui/qt/pixmaps/types/tape_16px.svg \
	gui/qt/pixmaps/types/type_directory.svg \
	gui/qt/pixmaps/types/type_file.svg \
	gui/qt/pixmaps/types/type_stream.svg \
	gui/qt/pixmaps/types/type_node.svg \
	gui/qt/pixmaps/types/type_playlist.svg \
	gui/qt/pixmaps/types/type_unknown.xpm \
	gui/qt/pixmaps/util/wait1.svg \
	gui/qt/pixmaps/util/wait2.svg \
	gui/qt/pixmaps/util/wait3.svg \
	gui/qt/pixmaps/util/wait4.svg \
	gui/qt/pixmaps/win7/win7thumbnail_pause.svg \
	gui/qt/pixmaps/win7/win7thumbnail_prev.svg \
	gui/qt/pixmaps/win7/win7thumbnail_next.svg \
	gui/qt/pixmaps/win7/win7thumbnail_play.svg \
	gui/qt/pixmaps/update.svg \
	gui/qt/pixmaps/valid.svg \
	gui/qt/pixmaps/search_clear.svg \
	gui/qt/pixmaps/lock.svg \
	gui/qt/pixmaps/redesign/1x_mobiledata.svg \
	gui/qt/pixmaps/redesign/add.svg \
	gui/qt/pixmaps/redesign/album.svg \
	gui/qt/pixmaps/redesign/arrow_back.svg \
	gui/qt/pixmaps/redesign/aspect_ratio.svg \
	gui/qt/pixmaps/redesign/av_timer.svg \
	gui/qt/pixmaps/redesign/blur_linear.svg \
	gui/qt/pixmaps/redesign/blur_on.svg \
	gui/qt/pixmaps/redesign/bookmark.svg \
	gui/qt/pixmaps/redesign/bookmarks.svg \
	gui/qt/pixmaps/redesign/build.svg \
	gui/qt/pixmaps/redesign/call_to_action.svg \
	gui/qt/pixmaps/redesign/cast.svg \
	gui/qt/pixmaps/redesign/check.svg \
	gui/qt/pixmaps/redesign/check_box_outline_blank.svg \
	gui/qt/pixmaps/redesign/chevron_left.svg \
	gui/qt/pixmaps/redesign/chevron_right.svg \
	gui/qt/pixmaps/redesign/close.svg \
	gui/qt/pixmaps/redesign/code.svg \
	gui/qt/pixmaps/redesign/compress.svg \
	gui/qt/pixmaps/redesign/computer.svg \
	gui/qt/pixmaps/redesign/cone.svg \
	gui/qt/pixmaps/redesign/construction.svg \
	gui/qt/pixmaps/redesign/content_copy.svg \
	gui/qt/pixmaps/redesign/content_paste.svg \
	gui/qt/pixmaps/redesign/create_new_folder.svg \
	gui/qt/pixmaps/redesign/crop.svg \
	gui/qt/pixmaps/redesign/crop_square.svg \
	gui/qt/pixmaps/redesign/dashboard_customize.svg \
	gui/qt/pixmaps/redesign/delete.svg \
	gui/qt/pixmaps/redesign/dns.svg \
	gui/qt/pixmaps/redesign/dock_to_bottom.svg \
	gui/qt/pixmaps/redesign/dock_to_right.svg \
	gui/qt/pixmaps/redesign/draft.svg \
	gui/qt/pixmaps/redesign/edit.svg \
	gui/qt/pixmaps/redesign/equalizer.svg \
	gui/qt/pixmaps/redesign/error.svg \
	gui/qt/pixmaps/redesign/event_note.svg \
	gui/qt/pixmaps/redesign/expand_more.svg \
	gui/qt/pixmaps/redesign/extension.svg \
	gui/qt/pixmaps/redesign/fast_rewind_fill.svg \
	gui/qt/pixmaps/redesign/fiber_manual_record.svg \
	gui/qt/pixmaps/redesign/fiber_manual_record_fill.svg \
	gui/qt/pixmaps/redesign/file_copy.svg \
	gui/qt/pixmaps/redesign/file_open.svg \
	gui/qt/pixmaps/redesign/first_page.svg \
	gui/qt/pixmaps/redesign/fit_screen.svg \
	gui/qt/pixmaps/redesign/folder.svg \
	gui/qt/pixmaps/redesign/folder_open.svg \
	gui/qt/pixmaps/redesign/forward_10.svg \
	gui/qt/pixmaps/redesign/fullscreen.svg \
	gui/qt/pixmaps/redesign/fullscreen_exit.svg \
	gui/qt/pixmaps/redesign/graphic_eq.svg \
	gui/qt/pixmaps/redesign/graphic_eq_fill.svg \
	gui/qt/pixmaps/redesign/help.svg \
	gui/qt/pixmaps/redesign/history.svg \
	gui/qt/pixmaps/redesign/home_fill.svg \
	gui/qt/pixmaps/redesign/info.svg \
	gui/qt/pixmaps/redesign/input.svg \
	gui/qt/pixmaps/redesign/keep.svg \
	gui/qt/pixmaps/redesign/keyboard.svg \
	gui/qt/pixmaps/redesign/keyboard_tab.svg \
	gui/qt/pixmaps/redesign/lan_fill.svg \
	gui/qt/pixmaps/redesign/language.svg \
	gui/qt/pixmaps/redesign/laps.svg \
	gui/qt/pixmaps/redesign/last_page.svg \
	gui/qt/pixmaps/redesign/layers.svg \
	gui/qt/pixmaps/redesign/library_music.svg \
	gui/qt/pixmaps/redesign/library_music_fill.svg \
	gui/qt/pixmaps/redesign/live_tv.svg \
	gui/qt/pixmaps/redesign/logout.svg \
	gui/qt/pixmaps/redesign/memory.svg \
	gui/qt/pixmaps/redesign/menu.svg \
	gui/qt/pixmaps/redesign/minimize.svg \
	gui/qt/pixmaps/redesign/movie.svg \
	gui/qt/pixmaps/redesign/movie_fill.svg \
	gui/qt/pixmaps/redesign/music_note.svg \
	gui/qt/pixmaps/redesign/note_add.svg \
	gui/qt/pixmaps/redesign/open_in_full.svg \
	gui/qt/pixmaps/redesign/palette.svg \
	gui/qt/pixmaps/redesign/pause.svg \
	gui/qt/pixmaps/redesign/pause_fill.svg \
	gui/qt/pixmaps/redesign/photo_camera.svg \
	gui/qt/pixmaps/redesign/picture_in_picture_alt.svg \
	gui/qt/pixmaps/redesign/play_arrow.svg \
	gui/qt/pixmaps/redesign/play_arrow_fill.svg \
	gui/qt/pixmaps/redesign/playlist_add_check.svg \
	gui/qt/pixmaps/redesign/playlist_play.svg \
	gui/qt/pixmaps/redesign/playlist_remove.svg \
	gui/qt/pixmaps/redesign/podcasts.svg \
	gui/qt/pixmaps/redesign/radio.svg \
	gui/qt/pixmaps/redesign/remove.svg \
	gui/qt/pixmaps/redesign/repeat.svg \
	gui/qt/pixmaps/redesign/repeat_one.svg \
	gui/qt/pixmaps/redesign/replay.svg \
	gui/qt/pixmaps/redesign/replay_10.svg \
	gui/qt/pixmaps/redesign/rss_feed.svg \
	gui/qt/pixmaps/redesign/save.svg \
	gui/qt/pixmaps/redesign/schedule.svg \
	gui/qt/pixmaps/redesign/search.svg \
	gui/qt/pixmaps/redesign/settings.svg \
	gui/qt/pixmaps/redesign/shuffle.svg \
	gui/qt/pixmaps/redesign/skip_next.svg \
	gui/qt/pixmaps/redesign/skip_next_fill.svg \
	gui/qt/pixmaps/redesign/skip_previous.svg \
	gui/qt/pixmaps/redesign/skip_previous_fill.svg \
	gui/qt/pixmaps/redesign/slow_motion_video.svg \
	gui/qt/pixmaps/redesign/sort.svg \
	gui/qt/pixmaps/redesign/spatial_audio.svg \
	gui/qt/pixmaps/redesign/speaker.svg \
	gui/qt/pixmaps/redesign/speed.svg \
	gui/qt/pixmaps/redesign/stop.svg \
	gui/qt/pixmaps/redesign/stop_fill.svg \
	gui/qt/pixmaps/redesign/subtitles.svg \
	gui/qt/pixmaps/redesign/surround_sound.svg \
	gui/qt/pixmaps/redesign/sync_alt.svg \
	gui/qt/pixmaps/redesign/terminal.svg \
	gui/qt/pixmaps/redesign/title.svg \
	gui/qt/pixmaps/redesign/transform.svg \
	gui/qt/pixmaps/redesign/tune.svg \
	gui/qt/pixmaps/redesign/update.svg \
	gui/qt/pixmaps/redesign/upload_file.svg \
	gui/qt/pixmaps/redesign/video_library.svg \
	gui/qt/pixmaps/redesign/videocam.svg \
	gui/qt/pixmaps/redesign/view_list.svg \
	gui/qt/pixmaps/redesign/visibility.svg \
	gui/qt/pixmaps/redesign/volume_down.svg \
	gui/qt/pixmaps/redesign/volume_down_fill.svg \
	gui/qt/pixmaps/redesign/volume_off.svg \
	gui/qt/pixmaps/redesign/volume_off_fill.svg \
	gui/qt/pixmaps/redesign/volume_up.svg \
	gui/qt/pixmaps/redesign/volume_up_fill.svg \
	gui/qt/pixmaps/redesign/wallpaper.svg \
	gui/qt/pixmaps/redesign/web.svg \
	gui/qt/pixmaps/redesign/width.svg \
	gui/qt/pixmaps/redesign/zoom_in.svg
EXTRA_DIST += gui/qt/vlc.qrc $(libqt_plugin_la_RES) gui/qt/pixmaps/redesign/LICENSE

nodist_libqt_plugin_la_SOURCES += gui/qt/resources.cpp

if ENABLE_VLM
nodist_libqt_plugin_la_SOURCES += gui/qt/dialogs/vlm.moc.cpp
endif

gui/qt/resources.cpp: gui/qt/vlc.qrc $(libqt_plugin_la_RES)
	$(AM_V_GEN)$(RCC) -name vlc -o $@ $<

if ENABLE_QT
gui_LTLIBRARIES += libqt_plugin.la
BUILT_SOURCES += $(nodist_libqt_plugin_la_SOURCES)
endif
