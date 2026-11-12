/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2011 VideoLAN and AUTHORS
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include "main_interface.hpp"
#include "input_manager.hpp"                    // Creation
#include "actions_manager.hpp"                  // killInstance

#include "util/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget
#include "util/qt_dirs.hpp"                     // toNativeSeparators
#include "util/theme.hpp"

#include "components/interface_widgets.hpp"     // bgWidget, videoWidget
#include "components/toast.hpp"
#include "components/drop_overlay.hpp"
#include "components/video_overlays.hpp"
#include "components/controller.hpp"            // controllers
#include "components/title_bar.hpp"             // titleBar
#include "components/playlist/playlist.hpp"     // plWidget
#include "components/playlist/playlist_panel.hpp" // PlaylistPanel
#include "components/sidebar.hpp"               // Sidebar
#include "components/now_playing.hpp"           // now-playing bar, audio mode, mini player
#include "components/spectrum.hpp"              // SpectrumWidget::setTap
#include "util/breakpoints.hpp"                 // BreakpointWatcher
#include "dialogs/firstrun.hpp"                 // First Run
#include "dialogs/playlist.hpp"                 // PlaylistDialog

#include "menus.hpp"                            // Menu creation
#include "recents.hpp"                          // RecentItems when DnD
/* after the components: extended_panels.hpp #defines BANDS */
#include "dialogs/extended.hpp"                 // Esc closes the sheet first

#include <QCloseEvent>
#include <QKeyEvent>

#include <QUrl>
#include <QSize>
#include <QDate>
#include <QMimeData>
#include <QFileInfo>

#include <QWindow>
#include <QMenu>
#include <QMenuBar>
#include <QLabel>
#include <QStackedWidget>
#include <QScreen>
#include <QShortcut>
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDesktopWidget>
#ifdef _WIN32
#include <QFileInfo>
#endif

#ifndef QT_NO_STATUSBAR
# include <QStatusBar>
#endif

#if ! HAS_QT510 && defined(QT_HAS_X11)
# include <QX11Info>
# include <X11/Xlib.h>
#endif

#include <QTimer>

#include <vlc_actions.h>                    /* Wheel event */
#include <vlc_vout_display.h>               /* vout_thread_t and VOUT_ events */

// #define DEBUG_INTF

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfBossCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfRaiseMainCB( vlc_object_t *p_this, const char *psz_variable,
                           vlc_value_t old_val, vlc_value_t new_val,
                           void *param );

const QEvent::Type MainInterface::ToolbarsNeedRebuild =
        (QEvent::Type)QEvent::registerEventType();

namespace
{
    /* A button of the player controls: the deck and its immersive and FSC
     * copies, the now-playing bar, the mini player, the player header */
    bool inPlayerControls( const QWidget *w )
    {
        for( ; w; w = w->parentWidget() )
            if( qobject_cast<const AbstractController *>( w )
             || qobject_cast<const NowPlayingBar *>( w )
             || qobject_cast<const MiniPlayer *>( w )
             || w->objectName() == QLatin1String( "playerHeader" ) )
                return true;
        return false;
    }

    /* Buttons in the player's windows click on Enter too. Space on one of
     * the player controls is the player's (play/pause), as if nothing had
     * focus; anywhere else (sidebar, pages, sheets) it presses the focused
     * button, as Windows does. Dialogs and pop-ups keep their own keys. */
    class ButtonKeys : public QObject
    {
    public:
        explicit ButtonKeys( QWidget *_main ) : QObject( _main ), main( _main ) {}

    protected:
        bool eventFilter( QObject *obj, QEvent *e ) Q_DECL_OVERRIDE
        {
            if( e->type() != QEvent::KeyPress && e->type() != QEvent::KeyRelease )
                return false;
            QAbstractButton *button = qobject_cast<QAbstractButton *>( obj );
            if( !button )
                return false;
            const QWidget *w = button->window();
            if( qobject_cast<const QDialog *>( w ) || w->windowType() == Qt::Popup
             || ( w != main && w->parentWidget() != main && w->objectName() != "miniPlayer" ) )
                return false;
            QKeyEvent *ke = static_cast<QKeyEvent *>( e );
            if( ke->modifiers() & ~Qt::KeypadModifier )
                return false;
            if( ke->key() == Qt::Key_Space && inPlayerControls( button ) )
            {
                e->ignore();    /* on to the window, whose hotkeys get it */
                return true;
            }
            if( ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter )
            {
                if( e->type() == QEvent::KeyPress && !ke->isAutoRepeat() )
                    button->click();
                return true;
            }
            return false;
        }

    private:
        QWidget *main;
    };
}

MainInterface::MainInterface( intf_thread_t *_p_intf, bool customFrame ) : QVLCMW( _p_intf )
{
    /* Variables initialisation */
    bgWidget             = NULL;
    videoWidget          = NULL;
    playlistWidget       = NULL;
    playlistPanel        = NULL;
    sidebar              = NULL;
    breakpoints          = NULL;
    nowPlayingInfo       = NULL;
    nowPlayingBar        = NULL;
    audioPage            = NULL;
    miniPlayer           = NULL;
    stackCentralOldWidget= NULL;
    lastWinScreen        = NULL;
    sysTray              = NULL;
    fullscreenControls   = NULL;
    cryptedLabel         = NULL;
    controls             = NULL;
    inputC               = NULL;
    pausedBadge          = NULL;
    playerHeader         = NULL;
    buffering            = NULL;
    immersiveBar         = NULL;
    i_resumePanelGrowth  = 0;

    b_hideAfterCreation  = false; // --qt-start-minimized
    playlistVisible      = false;
    input_name           = "";
    b_interfaceFullScreen= false;
    b_hasPausedWhenMinimized = false;
    i_kc_offset          = false;
    b_maximizedView      = false;
    b_isWindowTiled      = false;
    b_altAlone           = false;
    b_nowPlayingSeen     = false;
    i_minimalSidebarW    = 0;
    b_browsing           = false;
    b_audioModeCheck     = false;

    /* Ask for Privacy */
    FirstRun::CheckAndRun( this, p_intf );

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/
    /* Main settings; the window takes the keys at startup, after Back and
     * on a click, but is no Tab stop of its own (no ring to show it) */
    setFocusPolicy( Qt::ClickFocus );
    setAcceptDrops( true );
    setWindowRole( "vlc-main" );
    setWindowIcon( QApplication::windowIcon() );
    setWindowOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );

    /* Does the interface resize to video size or the opposite */
    b_autoresize = var_InheritBool( p_intf, "qt-video-autoresize" );

    /* Are we in the enhanced always-video mode or not ? */
    b_minimalView = var_InheritBool( p_intf, "qt-minimal-view" );

    /* Do we want anoying popups or not */
    i_notificationSetting = var_InheritInteger( p_intf, "qt-notification" );

    /* */
    b_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );

    /* Set the other interface settings */
    settings = getSettings();

    /* */
    b_plDocked = getSettings()->value( "MainWindow/pl-dock-status", true ).toBool();

    /* Should the UI stays on top of other windows */
    b_interfaceOnTop = var_InheritBool( p_intf, "video-on-top" );

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();

    /************
     * Menu Bar *
     ************/
    titleBar = new TitleBar( this );
    titleBar->setCaptionButtonsVisible( customFrame );
    setMenuWidget( titleBar );
    VLCMenuBar::createMenuBar( this, p_intf );
    /* Shortcuts of a hidden menu bar's actions die (minimal view, full
     * screen): the window keeps the menus' actions to keep them alive */
    addActions( menuBar()->actions() );
    connect( THEMIM->getIM(), &InputManager::voutListChanged,
             THEDP, &DialogsProvider::destroyPopupMenu );

    createMainWidget( settings );
    /* the title bar's cone goes to the library */
    connect( titleBar, &TitleBar::libraryRequested,
             this, [this]() { sidebar->showPage( Sidebar::Home ); } );

#ifndef QT_NO_STATUSBAR
    /**************
     * Status Bar *
     **************/
    createStatusBar();
    setStatusBarVisibility( getSettings()->value( "MainWindow/status-bar-visible", false ).toBool() );
#endif

    /*********************************
     * Create the Systray Management *
     *********************************/
    initSystray();

    /*************************************************************
     * Connect the input manager to the GUI elements it manages  *
     * Beware initSystray did some connects on input manager too *
     *************************************************************/
    /**
     * Connects on nameChanged()
     * Those connects are different because options can impeach them to trigger.
     **/
    /* Main Interface statusbar */
    connect( THEMIM->getIM(), &InputManager::nameChanged,
             this, &MainInterface::setName );
    /* and title of the Main Interface*/
    if( var_InheritBool( p_intf, "qt-name-in-title" ) )
    {
        connect( THEMIM->getIM(), &InputManager::nameChanged,
                 this, &MainInterface::setVLCWindowsTitle );
    }
    connect( THEMIM, &MainInputManager::inputChanged, this, &MainInterface::onInputChanged );
    connect( THEMIM->getIM(), &InputManager::voutChanged, this, &MainInterface::onVOutChanged );
    /* Audio mode in place of the cone for audio-only playback, unless the
     * user went to the library meanwhile. Decided once the input plays:
     * its streams are known by then (a TS stream's video can come later,
     * the video view then takes over as it always did). */
    connect( THEMIM->getIM(), &InputManager::playingStatusChanged, this, [this]( int state ) {
        input_thread_t *input = THEMIM->getInput();
        if( state != PLAYING_S || !b_audioModeCheck || input == NULL )
            return;
        b_audioModeCheck = false;
        if( THEMIM->getIM()->hasVideo() || var_CountChoices( input, "video-es" ) > 0 )
            return;
        QWidget *current = stackCentralW->currentWidget();
        /* the user is in Preferences: it stays, the audio plays under it */
        if( current == sidebar->page( Sidebar::Preferences ) )
            return;
        if( current == bgWidget || ( isLibraryPage( current ) && !b_browsing ) )
            showTab( audioPage );
    } );
    /* a cleared playlist has nothing left to show in the bar, nor on the
     * player page (its last item and deck would stay): cleared while
     * stopped, or the input ends after it */
    const auto nothingLeft = [this]() {
        if( THEMIM->getIM()->hasInput() || !THEMIM->hasEmptyPlaylist() )
            return;
        b_nowPlayingSeen = false;
        if( onPlayerPage() )
            sidebar->showPage( Sidebar::Home );
        updateNowPlayingBar();
    };
    connect( THEMIM, &MainInputManager::playlistNotEmpty, this, nothingLeft );
    connect( THEMIM, &MainInputManager::inputChanged, this, nothingLeft );

    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    b_videoFullScreen = false;
    b_miniAfterFullScreen = false;
    connect( this, &MainInterface::askGetVideo,
             this, &MainInterface::getVideoSlot,
             Qt::BlockingQueuedConnection );
    connect( this, &MainInterface::askReleaseVideo,
             this, &MainInterface::releaseVideoSlot,
             Qt::BlockingQueuedConnection );
    connect( this, &MainInterface::askVideoOnTop, this, &MainInterface::setVideoOnTop );

    if( videoWidget )
    {
        if( b_autoresize )
        {
            connect( videoWidget, &VideoWidget::sizeChanged,
                     this, &MainInterface::videoSizeChanged );
        }
        connect( this, &MainInterface::askVideoToResize,
                 this, &MainInterface::setVideoSize );

        connect( this, &MainInterface::askVideoSetFullScreen,
                 this, &MainInterface::setVideoFullScreen );
        connect( this, &MainInterface::askHideMouse,
                 this, &MainInterface::setHideMouse );
    }

    connect( THEDP, &DialogsProvider::toolBarConfUpdated, this, &MainInterface::toolBarConfUpdated );
    /* the effects sheet and the docked playlist share the right side:
     * opening the sheet closes the playlist (ExtendedDialog does the reverse) */
    connect( THEDP, &DialogsProvider::panelShown, this, [this]( int action, bool shown ) {
        if( action == EXTENDED_ACTION && shown && playlistPanel && playlistPanel->isVisible() )
        {
            playlistVisible = false;
            showPlaylistPanel( false );
        }
    } );
    installEventFilter( this );
    connect( this, &MainInterface::miniPlayerToggled, this, &MainInterface::placeVideo );

    toast = new Toast( p_intf, this, stackCentralW );
    connect( stackCentralW, &QStackedWidget::currentChanged, toast,
             [this]() { toast->setOverVideo( videoWidget
                            && stackCentralW->currentWidget() == videoWidget ); } );
    dropOverlay = new DropOverlay( this );
    buffering = new BufferingOverlay( p_intf, this, stackCentralW );
    createVideoOverlays();
    if( var_InheritBool( p_intf, "qt-immersive" ) )
        createImmersiveBar();
    connect( stackCentralW, &QStackedWidget::currentChanged,
             this, &MainInterface::updateDeckPlacement );
    updateDeckPlacement();
    /* Preferences own the content area: the docked playlist steps aside
     * (playlistVisible keeps the choice) and is back on the way out */
    connect( stackCentralW, &QStackedWidget::currentChanged, this, [this]() {
        if( b_plDocked && !b_videoFullScreen )
            showPlaylistPanel( playlistVisible
                && stackCentralW->currentWidget() != sidebar->page( Sidebar::Preferences ) );
    } );

    connect( this, &MainInterface::askToQuit, THEDP, &DialogsProvider::quit );

    connect( this, &MainInterface::askBoss, this, &MainInterface::setBoss );
    connect( this, &MainInterface::askRaise, this, &MainInterface::setRaise );

    /* A->B loop: one shortcut for the window, not one per deck button (the
     * deck, the immersive bar and the FSC each have one, often hidden) */
    QShortcut *atob = new QShortcut( QKeySequence( qtr( MENU_KEY_ATOB ) ), this );
    qApp->installEventFilter( new ButtonKeys( this ) );
    connect( atob, &QShortcut::activated, THEMIM->getIM(), &InputManager::setAtoB );


    connect( THEDP, &DialogsProvider::releaseMouseEvents, this, &MainInterface::voutReleaseMouseEvents ) ;
    /** END of CONNECTS**/


    /************
     * Callbacks
     ************/
    var_AddCallback( p_intf->obj.libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_AddCallback( p_intf->obj.libvlc, "intf-boss", IntfBossCB, p_intf );
    var_AddCallback( p_intf->obj.libvlc, "intf-show", IntfRaiseMainCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( p_intf->obj.libvlc, "intf-popupmenu", PopupMenuCB, p_intf );


    /* Final Sizing, restoration and placement of the interface */
    if( settings->value( "MainWindow/playlist-visible", false ).toBool() )
        togglePlaylist();

    /* Before the restore and the show: a minimum set later resizes a
     * maximised window to it, a fake maximise */
    computeMinimumSize( b_minimalView );
    QVLCTools::restoreWidgetPosition( settings, this, minimumSize() );
    /* P6-6: the theme takes the scale of the screen the window is on */
    winId();
    connect( windowHandle(), &QWindow::screenChanged, this, [this]() { followScreen(); } );
    followScreen();

    b_interfaceFullScreen = isFullScreen();

    setVisible( !b_hideAfterCreation );

    /* Switch to minimal view if needed, must be called after the show() */
    if( b_minimalView )
        toggleMinimalView( true );

    /* keyboard focus starts on the window (its keys drive the player), not
     * on the first sidebar button: Tab reaches that */
    setFocus();
}

MainInterface::~MainInterface()
{
    /* Unsure we hide the videoWidget before destroying it */
    if( stackCentralOldWidget == videoWidget )
        showTab( bgWidget );

    if( videoWidget )
        releaseVideoSlot(true);

    /* Be sure to kill the actionsManager... Only used in the MI and control */
    ActionsManager::killInstance();

    /* Delete the FSC controller */
    delete fullscreenControls;

    /* the mini player saves its place when it hides */
    if( miniPlayer )
        miniPlayer->hide();
    delete miniPlayer;
    SpectrumWidget::setTap( p_intf, false );

    /* Save states */

    settings->beginGroup("MainWindow");
    settings->setValue( "pl-dock-status", b_plDocked );

    /* Save playlist state */
    settings->setValue( "playlist-visible", playlistVisible );

    settings->setValue( "adv-controls",
                        getControlsVisibilityStatus() & CONTROLS_ADVANCED );
    settings->setValue( "status-bar-visible", b_statusbarVisible );

    /* Save the stackCentralW sizes */
    settings->setValue( "bgSize", stackWidgetsSizes[bgWidget] );
    settings->setValue( "playlistSize", stackWidgetsSizes[playlistWidget] );
    settings->endGroup();

    /* Save this size */
    QVLCTools::saveWidgetPosition(settings, this);

    /* Unregister callbacks */
    var_DelCallback( p_intf->obj.libvlc, "intf-boss", IntfBossCB, p_intf );
    var_DelCallback( p_intf->obj.libvlc, "intf-show", IntfRaiseMainCB, p_intf );
    var_DelCallback( p_intf->obj.libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_DelCallback( p_intf->obj.libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    p_intf->p_sys->p_mi = NULL;
}

/* The design's smallest window, within the screen's work area; minimal
 * view keeps the old video-only minimum (a 0 height lets the layout
 * decide it). */
void MainInterface::computeMinimumSize( bool b_minimal )
{
    if( b_minimal )
    {
        setMinimumSize( 80, 0 );
        return;
    }
    const Theme *t = Theme::instance();
    const QSize work = QApplication::desktop()->availableGeometry( this ).size();
    /* at most half the work area high, so a quarter snap fits; the deck
     * spans the window, stock's 80 px beside it are gone */
    setMinimumSize( qMin( work.width(), qMax( t->px( RedesignTokens::Breakpoint::XSMinW ),
                                              controls->compactWidth() ) ),
                    qMin( work.height() / 2, t->px( RedesignTokens::Breakpoint::XSMinH ) ) );
}

/* Once per change of screen: the sizes there (Theme rescales every page),
 * then the decks rebuilt at them */
void MainInterface::followScreen()
{
    /* the screen Qt gave the window, the one screenChanged() reports */
    if( !windowHandle() || !Theme::instance()->setScreen( windowHandle()->screen() ) )
        return;
    computeMinimumSize( b_minimalView );
    toolBarConfUpdated();
}

QMenuBar *MainInterface::menuBar() const
{
    return titleBar->menuBar();
}

/*****************************
 *   Main UI handling        *
 *****************************/
void MainInterface::recreateToolbars()
{
    bool b_adv = getControlsVisibilityStatus() & CONTROLS_ADVANCED;

    delete controls;
    delete inputC;
    createDecks( b_adv );
    if( immersiveBar )
        immersiveBar->rebuild( b_adv );

    if( fullscreenControls )
    {
        delete fullscreenControls;
        createFSC();
        THEMIM->requestVoutUpdate();
    }

    setMinimalView( b_minimalView );
    /* the new seek rows know nothing of a paused input */
    THEMIM->getIM()->resendPosition();
}

/* The seek row and the deck below the stack and the now-playing bar, or
 * the deck on top (MainWindow/ToolbarPos); the seek row first: the tab
 * order is creation order */
void MainInterface::createDecks( bool adv )
{
    inputC = new InputControlsWidget( p_intf, this );
    controls = new ControlsWidget( p_intf, adv, this );
    const int deckRow = mainLayout->indexOf( nowPlayingBar ) + 1;
    mainLayout->insertWidget( deckRow, inputC );
    mainLayout->insertWidget( settings->value( "MainWindow/ToolbarPos", false ).toBool() ? 0 : deckRow + 1,
                              controls );
}

void MainInterface::createFSC()
{
    fullscreenControls = new FullscreenControllerWidget( p_intf, this );
    connect( fullscreenControls, &FullscreenControllerWidget::keyPressed,
             this, &MainInterface::handleKeyPress );
}

void MainInterface::createImmersiveBar()
{
    immersiveBar = new ImmersiveBar( p_intf, this, stackCentralW,
                                     getControlsVisibilityStatus() & CONTROLS_ADVANCED );
    immersiveBar->setHeader( playerHeader );
}

void MainInterface::reloadPrefs()
{
    i_notificationSetting = var_InheritInteger( p_intf, "qt-notification" );
    b_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );
    if( !var_InheritBool( p_intf, "qt-fs-controller" ) && fullscreenControls )
    {
        delete fullscreenControls;
        fullscreenControls = NULL;
    }
}

void MainInterface::createResumePanel( QWidget *w )
{
    resumePanel = new QWidget( w );
    resumePanel->hide();
    QHBoxLayout *resumePanelLayout = new QHBoxLayout( resumePanel );
    resumePanelLayout->setSpacing( 0 ); resumePanelLayout->setContentsMargins( 0, 0, 0, 0 );

    QLabel *continuePixmapLabel = new QLabel();
    auto tint = [this, continuePixmapLabel]() {
        const int h = fontMetrics().height();
        continuePixmapLabel->setPixmap( Theme::instance()->icon( RedesignIcons::IconHistory,
                                            Theme::Text2, QSize( h, h ) ).pixmap( h, h ) );
        const int pad = Theme::instance()->px( RedesignTokens::Resume::PadX );
        continuePixmapLabel->setContentsMargins( pad, 0, pad, 0 );
    };
    tint();
    connect( Theme::instance(), &Theme::changed, continuePixmapLabel, tint );

    QLabel *continueLabel = new QLabel( qtr( "Do you want to restart the playback where left off?") );

    QToolButton *cancel = new QToolButton( resumePanel );
    setupDeckButton( p_intf, cancel );
    cancel->setIcon( deckIcon( RedesignIcons::IconClose ) );
    cancel->setAccessibleName( qtr( "Close" ) );
    cancel->setToolTip( qtr( "Close" ) );

    QPushButton *ok = new QPushButton( qtr( "&Continue" )  );

    resumePanelLayout->addWidget( continuePixmapLabel );
    resumePanelLayout->addWidget( continueLabel );
    resumePanelLayout->addStretch( 1 );
    resumePanelLayout->addWidget( ok );
    resumePanelLayout->addWidget( cancel );

    resumeTimer = new QTimer( resumePanel );
    resumeTimer->setSingleShot( true );
    resumeTimer->setInterval( RedesignTokens::Resume::Hold );

    connect( resumeTimer, &QTimer::timeout, this, &MainInterface::hideResumePanel );
    connect( cancel, &QToolButton::clicked, this, &MainInterface::hideResumePanel );
    connect( THEMIM->getIM(), &InputManager::resumePlayback, this, &MainInterface::showResumePanel );
    BUTTONACT( ok, resumePlayback );

    w->layout()->addWidget( resumePanel );
}

void MainInterface::showResumePanel( int64_t _time ) {
    int setting = var_InheritInteger( p_intf, "qt-continue" );

    if( setting == 0 )
        return;

    i_resumeTime = _time;

    if( setting == 2)
        resumePlayback();
    else
    {
        /* the window grows once by the panel (not by the size a widget
         * never laid out has, nor again for the next item while it shows),
         * within the work area; hideResumePanel() takes back what it got */
        if( resumePanel->isHidden() && !isFullScreen() && !isMaximized() && !b_isWindowTiled )
        {
            const int room = QApplication::desktop()->availableGeometry( this ).bottom()
                           - frameGeometry().bottom();
            i_resumePanelGrowth = qBound( 0, resumePanel->sizeHint().height(), room );
            resizeWindow( width(), height() + i_resumePanelGrowth );
        }
        resumePanel->setVisible(true);
        resumeTimer->start();
    }
}

void MainInterface::hideResumePanel()
{
    /* isHidden(): the panel of a minimised window is not visible either */
    if( !resumePanel->isHidden() )
    {
        if( !isFullScreen() && !isMaximized() && !b_isWindowTiled )
            resizeWindow( width(), height() - i_resumePanelGrowth );
        i_resumePanelGrowth = 0;
        resumePanel->hide();
        resumeTimer->stop();
    }
}

void MainInterface::resumePlayback()
{
    if( THEMIM->getIM()->hasInput() ) {
        var_SetInteger( THEMIM->getInput(), "time", i_resumeTime );
    }
    hideResumePanel();
}

void MainInterface::onInputChanged( bool hasInput )
{
    if( hasInput )
    {
        b_audioModeCheck = true;
        /* a stream buffers before it plays: show the player at once, where
         * the buffering ring is (the prototype's startStream()); audio mode
         * then takes over from it as for a file. Files keep the page until
         * they play, as before.
         * shortcut: b_browsing can't tell a stream the user opened from the
         * playlist moving on, so one opened while browsing during playback
         * keeps the page (as audio mode does); clear it in the Open paths
         * if that matters. */
        input_item_t *item = input_GetItem( THEMIM->getInput() );
        char *uri = input_item_GetURI( item );
        QWidget *current = stackCentralW->currentWidget();
        if( uri && strncmp( uri, "file://", 7 ) && !b_browsing && isLibraryPage( current )
         && current != sidebar->page( Sidebar::Preferences ) )
            showTab( bgWidget );
        free( uri );
        /* a new audio output (device change) starts without the tap */
        SpectrumWidget::setTap( p_intf, true );
        if( !b_nowPlayingSeen )
        {
            b_nowPlayingSeen = true;
            updateNowPlayingBar();
        }
    }
    if( hasInput == false )
        return;
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( autoRaise == MainInterface::RAISE_NEVER )
        return;
    if( THEMIM->getIM()->hasVideo() == true )
    {
        if( ( autoRaise & MainInterface::RAISE_VIDEO ) == 0 )
            return;
    }
    else if ( ( autoRaise & MainInterface::RAISE_AUDIO ) == 0 )
        return;
    emit askRaise();
}

void MainInterface::onVOutChanged( bool video )
{
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( video && ( autoRaise & MainInterface::RAISE_VIDEO ) )
        emit askRaise();
}

void MainInterface::createMainWidget( QSettings *creationSettings )
{
    /* Create the main Widget and the mainLayout */
    QWidget *main = new QWidget;
    setCentralWidget( main );
    mainLayout = new QVBoxLayout( main );
    main->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->setSpacing( 0 ); mainLayout->setContentsMargins( 0, 0, 0, 0 );

    createResumePanel( main );
    /* */
    stackCentralW = new QVLCStackedWidget( main );

    /* Bg Cone */
    if ( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY
         && var_InheritBool( p_intf, "qt-icon-change" ) )
    {
        bgWidget = new EasterEggBackgroundWidget( p_intf );
        connect( this, SIGNAL( kc_pressed() ), bgWidget, SLOT( animate() ) );
    }
    else
        bgWidget = new BackgroundWidget( p_intf );

    stackCentralW->addWidget( bgWidget );
    if ( !var_InheritBool( p_intf, "qt-bgcone" ) )
        bgWidget->setWithArt( false );
    else
        if ( var_InheritBool( p_intf, "qt-bgcone-expands" ) )
            bgWidget->setExpandstoHeight( true );

    /* And video Outputs */
    if( var_InheritBool( p_intf, "embedded-video" ) )
    {
        videoWidget = new VideoWidget( p_intf, stackCentralW );
        /* the video's native window made its parents (and, by default,
         * their siblings: the deck, the pages) native child HWNDs once a
         * video played; Qt then sent the focus events of their widgets
         * for HWNDs that never hold the Windows focus, so screen readers
         * saw only the window and Tab looked dead. Only the video widget
         * stays native (siblings: an application attribute, qt.cpp). */
        videoWidget->setAttribute( Qt::WA_DontCreateNativeAncestors );
        stackCentralW->addWidget( videoWidget );
    }

    /* Sidebar and stack side by side, at the stack's former layout index */
    QWidget *body = new QWidget( main );
    QHBoxLayout *bodyLayout = new QHBoxLayout( body );
    bodyLayout->setContentsMargins( 0, 0, 0, 0 );
    bodyLayout->setSpacing( 0 );
    sidebar = new Sidebar( p_intf, body );
    bodyLayout->addWidget( sidebar );
    bodyLayout->addWidget( stackCentralW, 1 );
    mainLayout->insertWidget( 1, body );

    /* Audio mode and the now-playing bar (below the stack) share what
     * is playing; the bar takes the deck's place under the library */
    nowPlayingInfo = new NowPlayingInfo( p_intf, this );
    audioPage = new AudioModePage( p_intf, nowPlayingInfo, stackCentralW );
    stackCentralW->addWidget( audioPage );
    /* before the first decoder builds its filters (see onInputChanged) */
    SpectrumWidget::setTap( p_intf, true );
    connect( audioPage, &AudioModePage::libraryRequested,
             this, [this]() { sidebar->showPage( Sidebar::Home ); } );
    nowPlayingBar = new NowPlayingBar( p_intf, nowPlayingInfo, main );
    nowPlayingBar->hide();
    mainLayout->insertWidget( mainLayout->indexOf( body ) + 1, nowPlayingBar );
    connect( nowPlayingBar, &NowPlayingBar::expandRequested, this, &MainInterface::showNowPlaying );

    for( int i = 0; i < Sidebar::PageCount; i++ )
        stackCentralW->addWidget( sidebar->page( static_cast<Sidebar::Page>( i ) ) );
    connect( sidebar, &Sidebar::pageRequested, this, [this]( QWidget *page ) {
        page = backTarget( page );
        /* browsing during playback: the next track keeps the library open */
        b_browsing = THEMIM->getIM()->hasInput();
        showTab( page );
    } );
    connect( stackCentralW, &QStackedWidget::currentChanged, sidebar, [this]() {
        sidebar->setCurrentWidget( stackCentralW->currentWidget() );
        updateBackLabel();
    } );
    /* what Back goes to changes with the playback */
    connect( THEMIM, &MainInputManager::inputChanged, this, &MainInterface::updateBackLabel );
    connect( THEMIM->getIM(), &InputManager::voutChanged, this, &MainInterface::updateBackLabel );
    connect( stackCentralW, &QStackedWidget::currentChanged,
             this, &MainInterface::updateNowPlayingBar );
    /* the window opens on Home; bgWidget stays for cover art */
    stackCentralW->setCurrentWidget( sidebar->page( Sidebar::Home ) );

    breakpoints = new BreakpointWatcher( this );
    breakpoints->addTarget( sidebar );
    sidebar->setBreakpoint( breakpoints->current() );
    connect( breakpoints, &BreakpointWatcher::changed, sidebar, &Sidebar::setBreakpoint );
    audioPage->setBreakpoint( breakpoints->current() );
    connect( breakpoints, &BreakpointWatcher::changed, audioPage, &AudioModePage::setBreakpoint );

    stackWidgetsSizes[bgWidget] =
        creationSettings->value( "MainWindow/bgSize", QSize( 600, 0 ) ).toSize();
    /* Resize even if no-auto-resize, because we are at creation */
    resizeStack( stackWidgetsSizes[bgWidget].width(), stackWidgetsSizes[bgWidget].height() );

    createDecks( creationSettings->value( "MainWindow/adv-controls", false ).toBool() );

    /* Visualisation, disabled for now, they SUCK */
    #if 0
    visualSelector = new VisualSelector( p_intf );
    mainLayout->insertWidget( 0, visualSelector );
    visualSelector->hide();
    #endif


    /* Enable the popup menu in the MI */
    main->setContextMenuPolicy( Qt::CustomContextMenu );
    connect( main, &QWidget::customContextMenuRequested,
             THEDP, &DialogsProvider::setPopupMenu );

    if ( depth() > 8 ) /* 8bit depth has too many issues with opacity */
        /* Create the FULLSCREEN CONTROLS Widget */
        if( var_InheritBool( p_intf, "qt-fs-controller" ) )
            createFSC();

    if ( b_interfaceOnTop )
        setWindowFlags( windowFlags() | Qt::WindowStaysOnTopHint );
}

inline void MainInterface::initSystray()
{
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    bool b_systrayWanted = var_InheritBool( p_intf, "qt-system-tray" );

    if( var_InheritBool( p_intf, "qt-start-minimized") )
    {
        if( b_systrayAvailable )
        {
            b_systrayWanted = true;
            b_hideAfterCreation = true;
        }
        else
            msg_Err( p_intf, "cannot start minimized without system tray bar" );
    }

    if( b_systrayAvailable && b_systrayWanted )
        createSystray();
}

#ifndef QT_NO_STATUSBAR
inline void MainInterface::createStatusBar()
{
    /****************
     *  Status Bar  *
     ****************/
    /* Widgets Creation*/
    QStatusBar *statusBarr = statusBar();

    TimeLabel *timeLabel = new TimeLabel( p_intf );
    nameLabel = new ClickableQLabel();
    nameLabel->setTextInteractionFlags( Qt::TextSelectableByMouse
                                      | Qt::TextSelectableByKeyboard );
    SpeedLabel *speedLabel = new SpeedLabel( p_intf, this );

    /* Styling those labels */
    timeLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    nameLabel->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel);
    /* their hover and padding: the theme's style sheet (QStatusBar) */

    /* and adding those */
    statusBarr->addWidget( nameLabel, 8 );
    statusBarr->addPermanentWidget( speedLabel, 0 );
    statusBarr->addPermanentWidget( timeLabel, 0 );

    connect( nameLabel, SIGNAL( doubleClicked() ), THEDP, SLOT( epgDialog() ) );
    /* timeLabel behaviour:
       - double clicking opens the goto time dialog
       - right-clicking and clicking just toggle between remaining and
         elapsed time.*/
    connect( timeLabel, &TimeLabel::doubleClicked, THEDP, &DialogsProvider::gotoTimeDialog );

#ifndef QT_NO_STATUSBAR
    connect( THEMIM->getIM(), &InputManager::encryptionChanged,
             this, &MainInterface::showCryptedLabel );
#endif

    /* This shouldn't be necessary, but for somehow reason, the statusBarr
       starts at height of 20px and when a text is shown it needs more space.
       But, as the QMainWindow policy doesn't allow statusBar to change QMW's
       geometry, we need to force a height. If you have a better idea, please
       tell me -- jb
     */
    statusBarr->setFixedHeight( statusBarr->sizeHint().height() + 2 );
}
#endif

/**********************************************************************
 * Handling of sizing of the components
 **********************************************************************/

void MainInterface::debug()
{
#ifdef DEBUG_INTF
    if( controls ) {
        msg_Dbg( p_intf, "Controls size: %i - %i", controls->size().height(), controls->size().width() );
        msg_Dbg( p_intf, "Controls minimumsize: %i - %i", controls->minimumSize().height(), controls->minimumSize().width() );
        msg_Dbg( p_intf, "Controls sizeHint: %i - %i", controls->sizeHint().height(), controls->sizeHint().width() );
    }

    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    msg_Dbg( p_intf, "minimumsize: %i - %i", minimumSize().height(), minimumSize().width() );

    msg_Dbg( p_intf, "Stack size: %i - %i", stackCentralW->size().height(), stackCentralW->size().width() );
    msg_Dbg( p_intf, "Stack sizeHint: %i - %i", stackCentralW->sizeHint().height(), stackCentralW->sizeHint().width() );
    msg_Dbg( p_intf, "Central size: %i - %i", centralWidget()->size().height(), centralWidget()->size().width() );
#endif
}

/* The window width that leaves stackWidth to the stack, when everything but
 * the stack and the sidebar takes otherWidth: the sidebar's own width
 * depends on the window's breakpoint, so the remembered stack sizes stay
 * the same whatever the sidebar was when they were taken. */
int MainInterface::windowWidthFor( int stackWidth, int otherWidth, bool withSidebar ) const
{
    if( !withSidebar )
        return otherWidth + stackWidth;
    /* the current breakpoint first: when two fit (a narrow sidebar with a
     * wide stack, or the reverse), the window must not jump to the other */
    const BreakpointWatcher::Size current = BreakpointWatcher::fromWidth( width() );
    const int same = otherWidth + stackWidth + Sidebar::widthAt( current );
    if( BreakpointWatcher::fromWidth( same ) == current )
        return same;
    for( int bp = BreakpointWatcher::XS; bp <= BreakpointWatcher::L; bp++ )
    {
        const BreakpointWatcher::Size size = static_cast<BreakpointWatcher::Size>( bp );
        const int w = otherWidth + stackWidth + Sidebar::widthAt( size );
        if( BreakpointWatcher::fromWidth( w ) == size )
            return w;
    }
    return otherWidth + stackWidth + Sidebar::widthAt( BreakpointWatcher::L );
}

/* The stack and the docked side panels (playlist, Effects sheet) beside it:
 * a panel takes its width from the view, it never widens the window */
QSize MainInterface::stackAreaSize() const
{
    int w = stackCentralW->width();
    foreach( QWidget *panel, stackCentralW->parentWidget()->findChildren<QWidget *>(
                                 QString(), Qt::FindDirectChildrenOnly ) )
        if( panel != sidebar && panel != stackCentralW && panel->isVisible() )
            w += panel->width();
    return QSize( w, stackCentralW->height() );
}

/* w is the width of the stack area (see stackAreaSize) */
void MainInterface::resizeStack( int w, int h )
{
    if( !isFullScreen() && !isMaximized() && !b_isWindowTiled )
    {
        const bool withSidebar = !sidebar->isHidden();
        const int other = width() - stackAreaSize().width()
                        - ( withSidebar ? sidebar->width() : 0 );
        /* never past the work area, nor below what the sidebar and the
         * rows need (a small video squashed them) */
        const QSize frame = frameGeometry().size() - size();
        const QSize work = QApplication::desktop()->availableGeometry( this ).size() - frame;
        /* a docked panel (playlist, Effects) stays docked: below the S
         * breakpoint it would turn into a drawer over a small video, which
         * then shows letterboxed beside it instead */
        const bool panelDocked = stackAreaSize().width() > stackCentralW->width();
        const int minW = panelDocked
                       ? Theme::instance()->px( RedesignTokens::Breakpoint::S ) : 0;
        const QSize target = QSize( windowWidthFor( w, other, withSidebar ),
                                    height() - stackCentralW->height() + h )
                .expandedTo( QSize( minW, minimumSizeHint().height() ) )
                .boundedTo( work );
        resizeWindow( target.width(), target.height() );
    }
    debug();
}

inline void MainInterface::showVideo() { showTab( videoWidget ); }
inline void MainInterface::restoreStackOldWidget( bool video_closing )
            { showTab( stackCentralOldWidget, video_closing ); }

inline void MainInterface::showTab( QWidget *widget, bool video_closing )
{
    if ( !widget ) widget = bgWidget; /* trying to restore a null oldwidget */
#ifdef DEBUG_INTF
    if ( stackCentralOldWidget )
        msg_Dbg( p_intf, "Old stackCentralOldWidget %s at index %i",
                 stackCentralOldWidget->metaObject()->className(),
                 stackCentralW->indexOf( stackCentralOldWidget ) );
    msg_Dbg( p_intf, "ShowTab request for %s", widget->metaObject()->className() );
#endif
    if ( stackCentralW->currentWidget() == widget )
        return;

    /* fixing when the playlist has been undocked after being hidden.
       restoreStackOldWidget() is called when video stops but
       stackCentralOldWidget would still be pointing to playlist */
    if ( widget == playlistWidget && !isPlDocked() )
        widget = bgWidget;

    QWidget *leaving = stackCentralW->currentWidget();
    /* Preferences is a detour with its own way back (Sidebar::returnTo):
     * never the page a stopping video returns to, nor what it replaces */
    QWidget *prefs = sidebar->page( Sidebar::Preferences );
    if( leaving != prefs && widget != prefs )
        stackCentralOldWidget = leaving;
    if( !isFullScreen() )
        stackWidgetsSizes[leaving] = stackAreaSize();

    /* If we are playing video, embedded */
    if( !video_closing && videoWidget && THEMIM->getIM()->hasVideo() )
    {
        /* Video -> Playlist */
        if( videoWidget == leaving && widget == playlistWidget )
        {
            stackCentralW->removeWidget( videoWidget );
            videoWidget->show(); videoWidget->raise();
        }

        /* Playlist -> Video */
        if( playlistWidget == leaving && widget == videoWidget )
        {
            playlistWidget->artContainer->removeWidget( videoWidget );
            videoWidget->show(); videoWidget->raise();
            stackCentralW->addWidget( videoWidget );
        }

        /* Background or library page -> Video */
        if( playlistWidget != leaving && widget == videoWidget )
        {
            /* In rare case when video is started before the interface */
            if( playlistWidget != NULL )
                playlistWidget->artContainer->removeWidget( videoWidget );
            videoWidget->show(); videoWidget->raise();
            stackCentralW->addWidget( videoWidget );
            stackCentralW->setCurrentWidget( videoWidget );
        }
    }

    stackCentralW->setCurrentWidget( widget );
    /* a widget shown for the first time keeps the current size, and so does
     * every switch from or to a library page or Preferences: the sizes per
     * view are stock's, for its own views */
    const auto stockView = [this]( QWidget *w ) {
        return w == bgWidget || w == videoWidget || w == playlistWidget;
    };
    if( b_autoresize && stockView( leaving ) && stockView( widget )
     && stackWidgetsSizes.contains( widget ) )
        resizeStack( stackWidgetsSizes[widget].width(), stackWidgetsSizes[widget].height() );

#ifdef DEBUG_INTF
    msg_Dbg( p_intf, "Stack state changed to %s, index %i",
              stackCentralW->currentWidget()->metaObject()->className(),
              stackCentralW->currentIndex() );
    if( stackCentralOldWidget )
        msg_Dbg( p_intf, "New stackCentralOldWidget %s at index %i",
                 stackCentralOldWidget->metaObject()->className(),
                 stackCentralW->indexOf( stackCentralOldWidget ) );
#endif

    /* This part is done later, to account for the new pl size */
    if( !video_closing && videoWidget && THEMIM->getIM()->hasVideo() &&
        videoWidget == leaving && widget == playlistWidget )
    {
        playlistWidget->artContainer->addWidget( videoWidget );
        playlistWidget->artContainer->setCurrentWidget( videoWidget );
    }
}

void MainInterface::toggleFSC()
{
   if( !fullscreenControls ) return;

   IMEvent *eShow = new IMEvent( IMEvent::FullscreenControlToggle );
   QApplication::postEvent( fullscreenControls, eShow );
}

/****************************************************************************
 * Video Handling
 ****************************************************************************/

/**
 * NOTE:
 * You must not change the state of this object or other Qt UI objects,
 * from the video output thread - only from the Qt UI main loop thread.
 * All window provider queries must be handled through signals or events.
 * That's why we have all those emit statements...
 */
bool MainInterface::getVideo( struct vout_window_t *p_wnd,
                              unsigned int i_width, unsigned int i_height,
                              bool fullscreen )
{
    bool result;

    /* This is a blocking call signal. Results are stored directly in the
     * vout_window_t and boolean pointers. Beware of deadlocks! */
    emit askGetVideo( p_wnd, i_width, i_height, fullscreen, &result );
    return result;
}

void MainInterface::getVideoSlot( struct vout_window_t *p_wnd,
                                  unsigned i_width, unsigned i_height,
                                  bool fullscreen, bool *res )
{
    /* Hidden or minimized, activate */
    if( isHidden() || isMinimized() )
        toggleUpdateSystrayMenu();

    /* Request the videoWidget */
    if ( !videoWidget )
    {
        videoWidget = new VideoWidget( p_intf, stackCentralW );
        videoWidget->setAttribute( Qt::WA_DontCreateNativeAncestors );  /* see createMainWidget */
        stackCentralW->addWidget( videoWidget );
        createVideoOverlays();
    }
    *res = videoWidget->request( p_wnd );
    if( *res ) /* The videoWidget is available */
    {
        setVideoFullScreen( fullscreen );

        /* Consider the video active now */
        showVideo();

        /* Ask videoWidget to resize correctly, if we are in normal mode */
        if( b_autoresize ) {
#if HAS_QT56
            qreal factor = videoWidget->devicePixelRatioF();

            i_width = qRound( (qreal) i_width / factor );
            i_height = qRound( (qreal) i_height / factor );
#endif

            videoWidget->setSize( i_width, i_height );
        }
    }
}

/* Asynchronous call from the WindowClose function */
void MainInterface::releaseVideo( void )
{
    emit askReleaseVideo(false);
}

/* Function that is CONNECTED to the previous emit */
void MainInterface::releaseVideoSlot( bool forced )
{
    /* This function is called when the embedded video window is destroyed,
     * or in the rare case that the embedded window is still here but the
     * Qt interface exits. */
    assert( videoWidget );
    videoWidget->release( forced );
    setVideoOnTop( false );
    setVideoFullScreen( false );
    hideResumePanel();

    if( stackCentralW->currentWidget() == videoWidget )
        restoreStackOldWidget( true );
    else if( playlistWidget &&
             playlistWidget->artContainer->currentWidget() == videoWidget )
    {
        playlistWidget->artContainer->setCurrentIndex( 0 );
        stackCentralW->addWidget( videoWidget );
    }

    /* We don't want to have a blank video to popup */
    stackCentralOldWidget = bgWidget;
}

// The provided size is in physical pixels, coming from the core.
void MainInterface::setVideoSize( unsigned int w, unsigned int h )
{
    if (!isFullScreen() && !isMaximized() )
    {
        /* Resize video widget to video size, or keep it at the same
         * size. Call setSize() either way so that vout_window_ReportSize
         * will always get called.
         * If the video size is too large for the screen, resize it
         * to the screen size.
         */
        if (b_autoresize)
        {
            const QRect screen = QApplication::desktop()->availableGeometry( this );
#if HAS_QT56
            float factor = videoWidget->devicePixelRatioF();
#else
            float factor = 1.0f;
#endif
            if( (float)h / factor > screen.height() )
            {
                /* the stack's width in a window as wide as the screen */
                const bool withSidebar = !sidebar->isHidden();
                w = screen.width() - ( width() - stackAreaSize().width() );
                if( withSidebar )
                    w += sidebar->width() - Sidebar::widthAt(
                             BreakpointWatcher::fromWidth( screen.width() ) );
                h = screen.height();
                h -= titleBar->barHeight();
                if( !b_minimalView )
                {
                    if( controls->isVisible() )
                        h -= controls->height();
#ifndef QT_NO_STATUSBAR
                    if( statusBar()->isVisible() )
                        h -= statusBar()->height();
#endif
                    if( inputC->isVisible() )
                        h -= inputC->height();
                }
                if( !titleBar->hasCaptionButtons() )
                    h -= style()->pixelMetric(QStyle::PM_TitleBarHeight);
                h -= style()->pixelMetric(QStyle::PM_LayoutBottomMargin);
                h -= 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
            }
            else
            {
                // Convert the size in logical pixels
                w = qRound( (float)w / factor );
                h = qRound( (float)h / factor );
                msg_Dbg( p_intf, "Logical video size: %ux%u", w, h );
            }
            videoWidget->setSize( w, h );
        }
        else
            videoWidget->setSize( videoWidget->width(), videoWidget->height() );
    }
}

void MainInterface::videoSizeChanged( int w, int h )
{
    /* the video at its size, the docked side panels beside it (resizeStack
     * keeps the window in the work area) */
    if( !playlistWidget || playlistWidget->artContainer->currentWidget() != videoWidget )
        resizeStack( w + stackAreaSize().width() - stackCentralW->width(), h );
}

void MainInterface::setVideoFullScreen( bool fs )
{
    /* the video lives in the main window, hidden behind the mini player */
    if( fs && isMiniPlayerVisible() )
        toggleMiniPlayer();
    b_videoFullScreen = fs;
    if( fs )
    {
        int numscreen = var_InheritInteger( p_intf, "qt-fullscreen-screennumber" );

        if ( numscreen >= 0 && numscreen < QGuiApplication::screens().length() )
        {
            if( fullscreenControls )
                fullscreenControls->setTargetScreen( numscreen );

            QRect screenres = QGuiApplication::screens()[ numscreen ]->geometry();
            lastWinScreen = windowHandle()->screen();
            windowHandle()->setScreen(QGuiApplication::screens()[numscreen]);

            /* To be sure window is on proper-screen in xinerama */
            if( !screenres.contains( pos() ) )
            {
                lastWinPosition = pos();
                lastWinSize = size();
                msg_Dbg( p_intf, "Moving video to correct position");
                move( QPoint( screenres.x(), screenres.y() ) );
            }
        }

        /* from the playlist or a library page */
        if( stackCentralW->currentWidget() != videoWidget )
            showTab( videoWidget );

        /* we won't be able to get its windowed sized once in fullscreen, so update it now */
        stackWidgetsSizes[stackCentralW->currentWidget()] = stackAreaSize();

        /* */
        displayNormalView();
        setInterfaceFullScreen( true );
    }
    else
    {
        setMinimalView( b_minimalView );
        setInterfaceFullScreen( b_interfaceFullScreen );
        if( lastWinScreen != NULL )
            windowHandle()->setScreen(lastWinScreen);
        if( lastWinPosition.isNull() == false )
        {
            move( lastWinPosition );
            resizeWindow( lastWinSize.width(), lastWinSize.height() );
            lastWinPosition = QPoint();
            lastWinSize = QSize();
        }

    }
    videoWidget->sync();
    updateDeckPlacement();
    if( b_miniAfterFullScreen )
    {
        b_miniAfterFullScreen = false;
        if( !fs )
            toggleMiniPlayer();
    }
}

void MainInterface::setHideMouse( bool hide )
{
    videoWidget->setCursor( hide ? Qt::BlankCursor : Qt::ArrowCursor );
    /* the immersive layout's header goes with the deck instead */
    if( playerHeader && !immersiveBar )
        playerHeader->setCursorHidden( hide );
}

/* Slot to change the video always-on-top flag.
 * Emit askVideoOnTop() to invoke this from other thread. */
void MainInterface::setVideoOnTop( bool on_top )
{
    //don't apply changes if user has already sets its interface on top
    if ( b_interfaceOnTop )
        return;

    Qt::WindowFlags oldflags = windowFlags(), newflags;

    if( on_top )
        newflags = oldflags | Qt::WindowStaysOnTopHint;
    else
        newflags = oldflags & ~Qt::WindowStaysOnTopHint;
    if( newflags != oldflags && !b_videoFullScreen )
    {
        setWindowFlags( newflags );
        show(); /* necessary to apply window flags */
    }
}

/* The paused badge and the header follow the video widget, which may be
 * created late */
void MainInterface::createVideoOverlays()
{
    if( !videoWidget || pausedBadge )
        return;
    pausedBadge = new PausedBadge( p_intf, this, videoWidget );
    playerHeader = new PlayerHeader( p_intf, this, videoWidget, nowPlayingInfo );
    connect( playerHeader->backButton(), &QAbstractButton::clicked,
             this, [this]() { sidebar->showPage( Sidebar::Home ); } );
    if( immersiveBar )
        immersiveBar->setHeader( playerHeader );
}

bool MainInterface::onPlayerPage() const
{
    QWidget *page = stackCentralW->currentWidget();
    return page == bgWidget || ( videoWidget && page == videoWidget )
        || ( audioPage && page == audioPage );
}

/* The immersive layout floats the deck over the player view (video or cover
 * art) of a window. Library pages keep the rows; minimal view and video full
 * screen hide them as before, the latter showing the FSC. */
bool MainInterface::deckFloats() const
{
    return immersiveBar && !b_videoFullScreen && !b_minimalView && onPlayerPage();
}

void MainInterface::updateDeckPlacement()
{
    if( buffering )
        buffering->setOnPlayer( onPlayerPage() );
    const bool floats = deckFloats();
    if( !b_minimalView && !b_videoFullScreen )
        setDeckVisible( true );
    if( immersiveBar )
        immersiveBar->setActive( floats );
    if( playerHeader )
        playerHeader->setActive( stackCentralW->currentWidget() == videoWidget
                                 && !b_videoFullScreen && !b_minimalView );
}

void MainInterface::setImmersive( bool on )
{
    config_PutInt( p_intf, "qt-immersive", on );
    if( on == ( immersiveBar != NULL ) )
        return;
    if( on )
    {
        if( playerHeader )
            playerHeader->forgetCursor();
        createImmersiveBar();
    }
    else
    {
        delete immersiveBar;
        immersiveBar = NULL;
        /* what the bar published: the toast and the audio page get the
         * space back (not in its destructor: at close the stack goes first) */
        stackCentralW->setProperty( "subtitleLift", 0 );
        stackCentralW->setProperty( "immersiveInset", 0 );
    }
    updateDeckPlacement();
}

void MainInterface::setInterfaceAlwaysOnTop( bool on_top )
{
    b_interfaceOnTop = on_top;
    Qt::WindowFlags oldflags = windowFlags(), newflags;

    if( on_top )
        newflags = oldflags | Qt::WindowStaysOnTopHint;
    else
        newflags = oldflags & ~Qt::WindowStaysOnTopHint;
    if( newflags != oldflags && !b_videoFullScreen )
    {
        setWindowFlags( newflags );
        show(); /* necessary to apply window flags */
    }
}

/* Asynchronous call from WindowControl function */
int MainInterface::controlVideo( int i_query, va_list args )
{
    switch( i_query )
    {
    case VOUT_WINDOW_SET_SIZE:
    {
        unsigned int i_width  = va_arg( args, unsigned int );
        unsigned int i_height = va_arg( args, unsigned int );

        emit askVideoToResize( i_width, i_height );
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_STATE:
    {
        unsigned i_arg = va_arg( args, unsigned );
        unsigned on_top = i_arg & VOUT_WINDOW_STATE_ABOVE;

        emit askVideoOnTop( on_top != 0 );
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_FULLSCREEN:
    {
        bool b_fs = va_arg( args, int );

        emit askVideoSetFullScreen( b_fs );
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_HIDE_MOUSE:
    {
        bool b_hide = va_arg( args, int );

        emit askHideMouse( b_hide );
        return VLC_SUCCESS;
    }
    default:
        msg_Warn( p_intf, "unsupported control query" );
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Playlist, Visualisation and Menus handling
 *****************************************************************************/
/**
 * Toggle the playlist widget or dialog
 **/
void MainInterface::createPlaylist()
{
    PlaylistDialog *dialog = PlaylistDialog::getInstance( p_intf );

    if( b_plDocked )
    {
        playlistWidget = dialog->exportPlaylistWidget();
        stackCentralW->addWidget( playlistWidget );
        stackWidgetsSizes[playlistWidget] = settings->value( "playlistSize", QSize( 600, 300 ) ).toSize();
    }
    connect( dialog, &PlaylistDialog::visibilityChanged, this, &MainInterface::setPlaylistVisibility );
}

void MainInterface::togglePlaylist()
{
    if( !playlistWidget ) createPlaylist();

    PlaylistDialog *dialog = PlaylistDialog::getInstance( p_intf );
    if( b_plDocked )
    {
        /* the panel beside the content; the playlistWidget page stays in
         * the stack for the separate window, no longer shown here */
        /* from what is on screen: full screen hides the panel */
        playlistVisible = !( playlistPanel && playlistPanel->isVisible() );
        showPlaylistPanel( playlistVisible );
    }
    else
    {
        playlistVisible = !playlistVisible;
        if ( ! dialog->hasPlaylistWidget() )
            dialog->importPlaylistWidget( playlistWidget );
        if ( playlistVisible )
            dialog->show();
        else
            dialog->hide();
    }
    debug();
}

const Qt::Key MainInterface::kc[10] =
{
    Qt::Key_Up, Qt::Key_Up,
    Qt::Key_Down, Qt::Key_Down,
    Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right,
    Qt::Key_B, Qt::Key_A
};

void MainInterface::dockPlaylist( bool p_docked )
{
    if( b_plDocked == p_docked ) return;
    /* some extra check */
    if ( b_plDocked && !playlistWidget ) createPlaylist();

    b_plDocked = p_docked;
    PlaylistDialog *dialog = PlaylistDialog::getInstance( p_intf );

    if( !p_docked ) /* Previously docked */
    {
        /* playlistVisible was the panel's */
        showPlaylistPanel( false );

        /* repositioning the videowidget __before__ exporting the
           playlistwidget into the playlist dialog avoids two unneeded
           calls to the server in the qt library to reparent the underlying
           native window back and forth.
           For Wayland, this is mandatory since reparenting is not implemented.
           For X11 or Windows, this is just an optimization. */
        if( stackCentralW->currentWidget() == playlistWidget )
        {
            if ( videoWidget && THEMIM->getIM()->hasVideo() )
                showTab(videoWidget);
            else
                showTab(bgWidget);
        }

        /* playlistwidget exported into the playlist dialog */
        stackCentralW->removeWidget( playlistWidget );
        dialog->importPlaylistWidget( playlistWidget );
        if ( playlistVisible ) dialog->show();
    }
    else /* Previously undocked */
    {
        playlistVisible = dialog->isVisible();
        dialog->hide();
        playlistWidget = dialog->exportPlaylistWidget();
        stackCentralW->addWidget( playlistWidget );

        /* If playlist is invisible don't show it */
        showPlaylistPanel( playlistVisible );
    }
}

/* The docked panel, created on first show; at XS a drawer over the stack */
void MainInterface::showPlaylistPanel( bool b_show )
{
    if( !playlistPanel )
    {
        if( !b_show )
            return;
        QBoxLayout *body = qobject_cast<QBoxLayout *>( stackCentralW->parentWidget()->layout() );
        assert( body );
        playlistPanel = new PlaylistPanel( p_intf, body, stackCentralW );
        playlistPanel->setBreakpoint( breakpoints->current() );
        connect( breakpoints, &BreakpointWatcher::changed,
                 playlistPanel, &PlaylistPanel::setBreakpoint );
        connect( playlistPanel, &PlaylistPanel::closeRequested,
                 this, &MainInterface::togglePlaylist );
    }
    playlistPanel->setOpen( b_show );
    THEDP->setPanelShown( PLAYLIST_ACTION, b_show );
}

/*
 * displayNormalView is the private function used by
 * the SLOT setVideoFullScreen to hide the controls;
 * setInterfaceFullScreen hides the title bar in video full screen
 */
void MainInterface::displayNormalView()
{
    sidebar->setVisible( false );
    showPlaylistPanel( false );   /* setMinimalView() brings it back */
    controls->setVisible( false );
    /* full screen from a library page: the bar shown there would stay
     * under the video, its controls Tab stops (setMinimalView() brings
     * it back) */
    nowPlayingBar->setVisible( false );
#ifndef QT_NO_STATUSBAR
    statusBar()->setVisible( false );
#endif
    inputC->setVisible( false );
    placeVideo();
}

/*
 * setMinimalView is the private function used by
 * the SLOT toggleMinimalView
 */
void MainInterface::setMinimalView( bool b_minimal )
{
    bool b_controlsVisible = controls->isVisible();
#ifndef QT_NO_STATUSBAR
    bool b_statusBarVisible = statusBar()->isVisible();
#endif
    bool b_inputCVisible = inputC->isVisible();
    const bool b_barVisible = nowPlayingBar->isVisible();
    /* the sidebar goes too (setDeckVisible): its width, and the stack's
     * for the way back */
    const bool b_sidebarShown = !sidebar->isHidden();
    const int i_stackWidth = stackCentralW->width();
    const int i_otherWidth = width() - i_stackWidth - ( b_sidebarShown ? sidebar->width() : 0 );

    /* The title bar becomes a strip with the window buttons, or nothing
     * under a native caption: its new height is known right away */
    int i_titleBarChange = titleBar->barHeight();
    titleBar->setMinimal( b_minimal );
    i_titleBarChange -= titleBar->barHeight();

    /* Lower the minimum before shrinking and raise it after growing, or it
     * would clamp the shrinking and add to the growth */
    if( b_minimal )
        computeMinimumSize( true );

    if( !isFullScreen() && !isMaximized() && b_minimal && !b_isWindowTiled )
    {
        int i_heightChange = i_titleBarChange;

        if( b_controlsVisible )
            i_heightChange += controls->height();
#ifndef QT_NO_STATUSBAR
        if( b_statusBarVisible )
            i_heightChange += statusBar()->height();
#endif
        if( b_inputCVisible )
            i_heightChange += inputC->height();
        if( b_barVisible )
            i_heightChange += nowPlayingBar->height();

        const int i_widthChange = b_sidebarShown ? sidebar->width() : 0;
        if( b_sidebarShown )   /* not again from a minimal view (full screen, toolbars) */
            i_minimalSidebarW = i_widthChange;
        if( i_heightChange != 0 || i_widthChange != 0 )
            resizeWindow( width() - i_widthChange, height() - i_heightChange );
    }

    if( b_plDocked )   /* back from full screen; minimal view keeps it */
        showPlaylistPanel( playlistVisible );
    setDeckVisible( !b_minimal );
#ifndef QT_NO_STATUSBAR
    statusBar()->setVisible( !b_minimal && b_statusbarVisible );
#endif

    if( !isFullScreen() && !isMaximized() && !b_minimal && !b_isWindowTiled )
    {
        int i_heightChange = -i_titleBarChange;

        if( !b_controlsVisible && controls->isVisible() )
            i_heightChange += controls->height();
#ifndef QT_NO_STATUSBAR
        if( !b_statusBarVisible && statusBar()->isVisible() )
            i_heightChange += statusBar()->height();
#endif
        if( !b_inputCVisible && inputC->isVisible() )
            i_heightChange += inputC->height();
        if( !b_barVisible && nowPlayingBar->isVisible() )
            i_heightChange += nowPlayingBar->height();

        /* the player pages have no sidebar to bring back; the one minimal
         * view took comes back as it was, when the width it gives still has
         * that sidebar (the current, sidebar-less width picks a narrower one) */
        const bool withSidebar = !onPlayerPage();
        const int i_back = width() + i_minimalSidebarW;
        int i_width = width();
        if( withSidebar && !b_sidebarShown )
            i_width = i_minimalSidebarW > 0 && Sidebar::widthAt(
                          BreakpointWatcher::fromWidth( i_back ) ) == i_minimalSidebarW
                    ? i_back : windowWidthFor( i_stackWidth, i_otherWidth, true );
        i_minimalSidebarW = 0;
        if( i_heightChange != 0 || i_width != width() )
            resizeWindow( i_width, height() + i_heightChange );
    }

    if( !b_minimal )
        computeMinimumSize( false );
    updateDeckPlacement();
}

void MainInterface::setDeckVisible( bool visible )
{
    /* a library page has the now-playing bar once something played, and
     * no deck before (inventory 1.1.1): the hotkeys work without it */
    const bool library = isLibraryPage( stackCentralW->currentWidget() );
    /* the player pages take the sidebar's width (renders 1.2.x): hidden
     * before the stack resizes (showTab runs this from currentChanged),
     * so the window keeps its width */
    sidebar->setVisible( visible && !onPlayerPage() );
    nowPlayingBar->setVisible( visible && library && b_nowPlayingSeen );
    /* in the immersive layout the deck floats over the video instead */
    controls->setVisible( visible && !library && !deckFloats() );
    inputC->setVisible( visible && !library && !deckFloats() );
    placeVideo();
}

/* I1-2: the one video window plays where it is seen: in the mini player,
 * in the now-playing bar's thumb on a library page, else on the player
 * page (full screen included). Audio keeps the art: no video, no move. */
void MainInterface::placeVideo()
{
    if( !videoWidget )
        return;
    QWidget *host = NULL;
    if( isMiniPlayerVisible() )
        host = miniPlayer->videoHost();
    else if( nowPlayingBar->isVisibleTo( this ) )
        host = nowPlayingBar->videoHost();
    videoWidget->setHost( host );
}

bool MainInterface::isLibraryPage( QWidget *w ) const
{
    for( int i = 0; i < Sidebar::PageCount; i++ )
        if( w == sidebar->page( static_cast<Sidebar::Page>( i ) ) )
            return true;
    return false;
}

QWidget *MainInterface::backTarget( QWidget *page ) const
{
    InputManager *im = THEMIM->getIM();
    if( !page || ( page == videoWidget && !im->hasVideo() )
     || ( ( page == audioPage || page == bgWidget ) && !im->hasInput() ) )
        return sidebar->page( Sidebar::Home );
    return page;
}

void MainInterface::updateBackLabel()
{
    if( stackCentralW->currentWidget() == sidebar->page( Sidebar::Preferences ) )
        sidebar->setBackLabel( isLibraryPage( backTarget( sidebar->returnTo() ) ) );
}

void MainInterface::updateNowPlayingBar()
{
    if( !isLibraryPage( stackCentralW->currentWidget() ) )
        b_browsing = false;
    /* not before the deck exists, and full screen keeps everything hidden */
    if( !controls || !inputC || b_videoFullScreen )
        return;
    setDeckVisible( !b_minimalView );
    /* now, so a resizeStack() right after sees the new stack height */
    centralWidget()->layout()->activate();
}

/* The bar's expand: the video when there is one, else audio mode */
void MainInterface::showNowPlaying()
{
    if( videoWidget && THEMIM->getIM()->hasVideo() )
        showVideo();
    else
        showTab( audioPage );
}

bool MainInterface::isMiniPlayerVisible() const
{
    return miniPlayer && miniPlayer->isVisible();
}

/* Hidden (tray, mini player), minimised, in video full screen or in minimal
 * view (no sidebar for its sections), the window can't show the page: the
 * caller opens the dialog as before */
bool MainInterface::showPreferences()
{
    if( !isVisible() || isMinimized() || b_videoFullScreen || isMiniPlayerVisible()
     || b_minimalView )
        return false;
    sidebar->showPage( Sidebar::Preferences );
    return true;
}

/* DESIGN_SPEC 9 / D3: the mini player stands in for the main window, which
 * hides (the video plays in the mini player, placeVideo()); restore brings
 * it back. */
void MainInterface::toggleMiniPlayer()
{
    if( !miniPlayer )
    {
        miniPlayer = new MiniPlayer( p_intf, nowPlayingInfo, NULL );
        connect( miniPlayer, &MiniPlayer::restoreRequested, this, &MainInterface::toggleMiniPlayer );
        /* the menus' shortcuts too (Ctrl+P, Ctrl+E...): this window is
         * hidden, so its own don't fire */
        connect( miniPlayer, &MiniPlayer::keyPressed, this, [this]( QKeyEvent *e ) {
            const QKeySequence key( int( e->modifiers() & ~Qt::KeypadModifier ) + e->key() );
            foreach( QAction *action, findChildren<QAction *>() )
                if( action->isEnabled() && action->shortcuts().contains( key ) )
                {
                    action->trigger();
                    e->accept();
                    return;
                }
            handleKeyPress( e );
        } );
    }
    const bool b_mini = !miniPlayer->isVisible();
    /* the video lives in this window: leave its full screen first, as the
     * hotkey does; setVideoFullScreen( false ) then comes back here */
    if( b_mini && b_videoFullScreen )
    {
        b_miniAfterFullScreen = true;
        var_SetBool( THEPL, "fullscreen", false );
        vout_thread_t *p_vout = THEMIM->getVout();
        if( p_vout )
        {
            var_SetBool( p_vout, "fullscreen", false );
            vlc_object_release( p_vout );
        }
        return;
    }
    if( b_mini )
    {
        miniPlayer->popIn();
        hide();
    }
    else
    {
        miniPlayer->hide();
        if( isMinimized() )
            showNormal();
        else
            show();
        raise();
        activateWindow();
    }
    emit miniPlayerToggled( b_mini );
}

/*
 * This public SLOT is used for moving to minimal View Mode
 *
 * If b_minimal is false, then we are normalView
 */
void MainInterface::toggleMinimalView( bool b_minimal )
{
    /* its sections are in the sidebar, which minimal view hides: leave it
     * as Back does (Ctrl+P there opens the dialog) */
    if( b_minimal && stackCentralW->currentWidget() == sidebar->page( Sidebar::Preferences ) )
        sidebar->goBack();
    if( !b_minimalView && b_autoresize ) /* Normal mode */
    {
        if( stackCentralW->currentWidget() == bgWidget )
        {
            if( stackCentralW->height() < 16 )
            {
                resizeStack( stackAreaSize().width(), 100 );
            }
        }
    }
    b_minimalView = b_minimal;
    if( !b_videoFullScreen )
        setMinimalView( b_minimalView );

    emit minimalViewToggled( b_minimalView );
}

/* toggling advanced controls buttons */
void MainInterface::toggleAdvancedButtons()
{
    controls->toggleAdvanced();
    /* the expert row widens the deck: the window's minimum follows */
    if( !b_minimalView )
        computeMinimumSize( false );
    if( immersiveBar )
        immersiveBar->rebuild( controls->b_advancedVisible );
//    if( fullscreenControls ) fullscreenControls->toggleAdvanced();
}

/* Get the visibility status of the controls (hidden or not, advanced or not) */
int MainInterface::getControlsVisibilityStatus()
{
    if( !controls ) return 0;
    /* the minimal view, not the deck's visibility: the now-playing bar, the
     * immersive bar and full screen hide the deck too */
    return( ( b_minimalView ? CONTROLS_HIDDEN : CONTROLS_VISIBLE )
            + CONTROLS_ADVANCED * controls->b_advancedVisible );
}

/* Get whether the advanced buttons widget is available
 * (if its actually present in any of the configurable locations).
 */
bool MainInterface::isAdvancedWidgetAvailable()
{
    if( !controls) return false;
    return controls->advancedAvailable();
}

StandardPLPanel *MainInterface::getPlaylistView()
{
    if( !playlistWidget ) return NULL;
    else return playlistWidget->mainView;
}

void MainInterface::setStatusBarVisibility( bool b_visible )
{
#ifndef QT_NO_STATUSBAR
    statusBar()->setVisible( b_visible );
    b_statusbarVisible = b_visible;
#endif
}


void MainInterface::setPlaylistVisibility( bool b_visible )
{
    if( isPlDocked() || THEDP->isDying() || (playlistWidget && playlistWidget->isMinimized() ) )
        return;

    playlistVisible = b_visible;
}

/************************************************************************
 * Other stuff
 ************************************************************************/
void MainInterface::setName( const QString& name )
{
    input_name = name; /* store it for the QSystray use */
#ifndef QT_NO_STATUSBAR
    /* Display it in the status bar, but also as a Tooltip in case it doesn't
       fit in the label */
    nameLabel->setText( name );
    nameLabel->setToolTip( name );
#endif // QT_NO_STATUSBAR
}

/**
 * Give the decorations of the Main Window a correct Name.
 * If nothing is given, set it to VLC...
 **/
void MainInterface::setVLCWindowsTitle( const QString& aTitle )
{
    if( aTitle.isEmpty() )
    {
        setWindowTitle( qtr( "VLC media player" ) );
    }
    else
    {
        setWindowTitle( aTitle + " - " + qtr( "VLC media player" ) );
    }
}

void MainInterface::showCryptedLabel( bool b_show )
{
#ifndef QT_NO_STATUSBAR
    if( cryptedLabel == NULL )
    {
        cryptedLabel = new QLabel;
        // The lock icon is not the right one for DRM protection/scrambled.
        //cryptedLabel->setPixmap( QPixmap( ":/lock.svg" ) );
        cryptedLabel->setText( "DRM" );
#ifndef QT_NO_STATUSBAR
        statusBar()->addWidget( cryptedLabel );
#endif
    }

    cryptedLabel->setVisible( b_show );
#endif
}

/*****************************************************************************
 * Systray Icon and Systray Menu
 *****************************************************************************/
/**
 * Create a SystemTray icon and a menu that would go with it.
 * Connects to a click handler on the icon.
 **/
void MainInterface::createSystray()
{
    QIcon iconVLC;
    if( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY && var_InheritBool( p_intf, "qt-icon-change" ) )
        iconVLC = QIcon::fromTheme( "vlc-xmas", QIcon( ":/logo/vlc128-xmas.png" ) );
    else
        iconVLC = QIcon::fromTheme( "vlc", QIcon( ":/logo/vlc256.png" ) );
    sysTray = new QSystemTrayIcon( iconVLC, this );
    sysTray->setToolTip( qtr( "VLC media player" ));

    systrayMenu = new QMenu( qtr( "VLC media player" ), this );
    systrayMenu->setIcon( iconVLC );

    VLCMenuBar::updateSystrayMenu( this, p_intf, true );
    sysTray->show();

    connect( sysTray, &QSystemTrayIcon::activated,
             this, &MainInterface::handleSystrayClick );

    /* Connects on nameChanged() */
    connect( THEMIM->getIM(), &InputManager::nameChanged,
             this, &MainInterface::updateSystrayTooltipName );
    /* Connect PLAY_STATUS on the systray */
    connect( THEMIM->getIM(), &InputManager::playingStatusChanged,
             this, &MainInterface::updateSystrayTooltipStatus );
}

void MainInterface::toggleUpdateSystrayMenuWhenVisible(bool)
{
    hide();
}

void MainInterface::resizeWindow(int w, int h)
{
#if ! HAS_QT510 && defined(QT_HAS_X11)
    if( QX11Info::isPlatformX11() )
    {
#if HAS_QT56
        qreal dpr = devicePixelRatioF();
#else
        qreal dpr = devicePixelRatio();
#endif
        QSize size(w, h);
        size = size.boundedTo(maximumSize()).expandedTo(minimumSize());
        /* X11 window managers are not required to accept geometry changes on
         * the top-level window.  Unfortunately, Qt < 5.10 assumes that the
         * change will succeed, and resizes all sub-windows unconditionally.
         * By calling XMoveResizeWindow directly, Qt will not see our change
         * request until the ConfigureNotify event on success
         * and not at all if it is rejected. */
        XResizeWindow( QX11Info::display(), winId(),
                       (unsigned int)size.width() * dpr, (unsigned int)size.height() * dpr);
        return;
    }
#endif
    resize(w, h);
}

/**
 * Updates the Systray Icon's menu and toggle the main interface
 */
void MainInterface::toggleUpdateSystrayMenu(bool requestActivate)
{
    /* If hidden, show it */
    if( requestActivate && isHidden() )
    {
        show();
        activateWindow();
    }
    else if( requestActivate && isMinimized() )
    {
        /* Minimized */
        showNormal();
        activateWindow();
    }
    else if ( !isHidden() && !isMinimized() )
    {
        /* Visible (possibly under other windows) */
        toggleUpdateSystrayMenuWhenVisible(requestActivate);
    }
    if( sysTray )
        VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* First Item of the systray menu */
void MainInterface::showUpdateSystrayMenu()
{
    if( isHidden() )
        show();
    if( isMinimized() )
        showNormal();
    activateWindow();

    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* First Item of the systray menu */
void MainInterface::hideUpdateSystrayMenu()
{
    hide();
    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* Click on systray Icon */
void MainInterface::handleSystrayClick(
                                    QSystemTrayIcon::ActivationReason reason )
{
    switch( reason )
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
#ifdef Q_OS_MAC
            VLCMenuBar::updateSystrayMenu( this, p_intf );
#else
            toggleUpdateSystrayMenu(true);
#endif
            break;
        case QSystemTrayIcon::MiddleClick:
            sysTray->showMessage( qtr( "VLC media player" ),
                    qtr( "Control menu for the player" ),
                    QSystemTrayIcon::Information, 3000 );
            break;
        default:
            break;
    }
}

/**
 * Updates the name of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void MainInterface::updateSystrayTooltipName( const QString& name )
{
    if( name.isEmpty() )
    {
        sysTray->setToolTip( qtr( "VLC media player" ) );
    }
    else
    {
        sysTray->setToolTip( name );
        if( ( i_notificationSetting == NOTIFICATION_ALWAYS ) ||
            ( i_notificationSetting == NOTIFICATION_MINIMIZED && (isMinimized() || isHidden()) ) )
        {
            sysTray->showMessage( qtr( "VLC media player" ), name,
                    QSystemTrayIcon::NoIcon, 3000 );
        }
    }

    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/**
 * Updates the status of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void MainInterface::updateSystrayTooltipStatus( int i_status )
{
    switch( i_status )
    {
    case PLAYING_S:
        sysTray->setToolTip( input_name );
        break;
    case PAUSE_S:
        sysTray->setToolTip( input_name + " - " + qtr( "Paused") );
        break;
    default:
        sysTray->setToolTip( qtr( "VLC media player" ) );
        break;
    }
    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

void MainInterface::changeEvent(QEvent *event)
{
    if( event->type() == QEvent::WindowStateChange )
    {
        QWindowStateChangeEvent *windowStateChangeEvent = static_cast<QWindowStateChangeEvent*>(event);
        Qt::WindowStates newState = windowState();
        Qt::WindowStates oldState = windowStateChangeEvent->oldState();

        /* b_maximizedView stores if the window was maximized before entering fullscreen.
         * It is set when entering maximized mode, unset when leaving it to normal mode.
         * Upon leaving full screen, if b_maximizedView is set,
         * the window should be maximized again. */
        if( newState & Qt::WindowMaximized &&
            !( oldState & Qt::WindowMaximized ) )
            b_maximizedView = true;

        if( !( newState & Qt::WindowMaximized ) &&
            oldState & Qt::WindowMaximized &&
            !b_videoFullScreen )
            b_maximizedView = false;

        if( !( newState & Qt::WindowFullScreen ) &&
            oldState & Qt::WindowFullScreen &&
            b_maximizedView )
        {
            showMaximized();
            return;
        }

        if( newState & Qt::WindowMinimized )
        {
            b_hasPausedWhenMinimized = false;

            if( THEMIM->getIM()->playingStatus() == PLAYING_S &&
                THEMIM->getIM()->hasVideo() && !THEMIM->getIM()->hasVisualisation() &&
                b_pauseOnMinimize )
            {
                b_hasPausedWhenMinimized = true;
                THEMIM->pause();
            }
        }
        else if( oldState & Qt::WindowMinimized && !( newState & Qt::WindowMinimized ) )
        {
            if( b_hasPausedWhenMinimized )
            {
                THEMIM->play();
            }
        }
    }

    QWidget::changeEvent(event);
}

/************************************************************************
 * D&D Events
 ************************************************************************/
void MainInterface::dropEvent(QDropEvent *event)
{
    /* before the drop starts playback, and on every early return */
    dropOverlay->setActive( false );
    dropEventPlay( event, true );
}

void MainInterface::showToast( const QString &message )
{
    toast->showMessage( message );
}

/**
 * dropEventPlay
 *
 * Event called if something is dropped onto a VLC window
 * \param event the event in question
 * \param b_play whether to play the file immediately
 * \param b_playlist true to add to playlist, false to add to media library
 * \return nothing
 */
void MainInterface::dropEventPlay( QDropEvent *event, bool b_play, bool b_playlist )
{
    if( event->possibleActions() & ( Qt::CopyAction | Qt::MoveAction | Qt::LinkAction ) )
       event->setDropAction( Qt::CopyAction );
    else
        return;

    const QMimeData *mimeData = event->mimeData();

    /* D&D of a subtitles file, add it on the fly */
    if( mimeData->urls().count() == 1 && THEMIM->getIM()->hasInput() )
    {
        if( !input_AddSlave( THEMIM->getInput(), SLAVE_TYPE_SPU,
                 qtu( mimeData->urls()[0].toString() ), true, false, true ) )
        {
            showToast( qtr( "Loaded subtitle: %1" )
                       .arg( QFileInfo( mimeData->urls()[0].path() ).fileName() ) );
            event->accept();
            return;
        }
    }

    bool first = b_play;
    foreach( const QUrl &url, mimeData->urls() )
    {
        if( url.isValid() )
        {
            QString mrl = toURI( url.toEncoded().constData() );
#ifdef _WIN32
            QFileInfo info( url.toLocalFile() );
            if( info.exists() && info.isSymLink() )
            {
                QString target = info.symLinkTarget();
                QUrl url;
                if( QFile::exists( target ) )
                {
                    url = QUrl::fromLocalFile( target );
                }
                else
                {
                    url.setUrl( target );
                }
                mrl = toURI( url.toEncoded().constData() );
            }
#endif
            if( mrl.length() > 0 )
            {
                Open::openMRL( p_intf, mrl, first, b_playlist );
                first = false;
            }
        }
    }

    /* Browsers give content as text if you dnd the addressbar,
       so check if mimedata has valid url in text and use it
       if we didn't get any normal Urls()*/
    if( !mimeData->hasUrls() && mimeData->hasText() &&
        QUrl(mimeData->text()).isValid() )
    {
        QString mrl = toURI( mimeData->text() );
        Open::openMRL( p_intf, mrl, first, b_playlist );
    }
    event->accept();
}
void MainInterface::dragEnterEvent(QDragEnterEvent *event)
{
     dropOverlay->setActive( event->mimeData()->hasUrls() );
     event->acceptProposedAction();
}
void MainInterface::dragMoveEvent(QDragMoveEvent *event)
{
     event->acceptProposedAction();
}
void MainInterface::dragLeaveEvent(QDragLeaveEvent *event)
{
     dropOverlay->setActive( false );
     event->accept();
}

/************************************************************************
 * Events stuff
 ************************************************************************/
void MainInterface::keyPressEvent( QKeyEvent *e )
{
    b_altAlone = e->key() == Qt::Key_Alt && e->modifiers() == Qt::AltModifier;
    handleKeyPress( e );

    /* easter eggs sequence handling */
    if ( e->key() == kc[ i_kc_offset ] )
        i_kc_offset++;
    else
        i_kc_offset = 0;

    if ( i_kc_offset == (sizeof( kc ) / sizeof( Qt::Key )) )
    {
        i_kc_offset = 0;
        emit kc_pressed();
    }
}

void MainInterface::keyReleaseEvent( QKeyEvent *e )
{
    /* Alt alone shows the menus that minimal view hides (DESIGN_SPEC 2) */
    if( e->key() == Qt::Key_Alt && b_altAlone && b_minimalView )
    {
        titleBar->toggleMenus();
        e->accept();
    }
    else
        QVLCMW::keyReleaseEvent( e );
    b_altAlone = false;
}

void MainInterface::handleKeyPress( QKeyEvent *e )
{
    /* Esc closes the top-most surface first: the Effects sheet, the
     * playlist drawer, then Preferences (back), then minimal view or
     * (key-leave-fullscreen) the full screen. Menus, pop-ups and dialogs
     * are windows of their own and take it before. */
    if( e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier
     && THEDP->isPanelShown( EXTENDED_ACTION ) )
    {
        ExtendedDialog::getInstance( p_intf )->hide();
        e->accept();
        return;
    }
    if( e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier
     && playlistPanel && playlistPanel->isWindow() && playlistPanel->isVisible() )
    {
        togglePlaylist();   /* the XS drawer, over the page */
        e->accept();
        return;
    }
    if( e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier
     && isVisible() /* not from the mini player */
     && stackCentralW->currentWidget() == sidebar->page( Sidebar::Preferences ) )
    {
        sidebar->goBack();
        e->accept();
        return;
    }
    if( ( ( e->modifiers() & Qt::ControlModifier ) && ( e->key() == Qt::Key_H ) ) ||
        ( b_minimalView && !b_videoFullScreen && e->key() == Qt::Key_Escape ) )
    {
        toggleMinimalView( !b_minimalView );
        e->accept();
    }
    else if( ( e->modifiers() & Qt::ControlModifier ) && ( e->key() == Qt::Key_K ) &&
        playlistWidget )
    {
        playlistWidget->setSearchFieldFocus();
        e->accept();
    }

    int i_vlck = qtEventToVLCKey( e );
    if( i_vlck > 0 )
    {
        var_SetInteger( p_intf->obj.libvlc, "key-pressed", i_vlck );
        e->accept();
    }
    else
        e->ignore();
}

void MainInterface::wheelEvent( QWheelEvent *e )
{
    /* the volume follows the wheel over the player and the deck, as over
     * stock's window; a page or a docked panel (Effects, playlist) passes
     * on a wheel it can't use (a list at its end), which it keeps */
    QWidget *w = childAt( e->pos() );
    const bool over = w && ( ( onPlayerPage() && stackCentralW->isAncestorOf( w ) )
        || ( controls && ( w == controls || controls->isAncestorOf( w ) ) )
        || ( inputC && ( w == inputC || inputC->isAncestorOf( w ) ) )
        || ( nowPlayingBar && ( w == nowPlayingBar || nowPlayingBar->isAncestorOf( w ) ) ) );
    if( !over )
    {
        e->accept();
        return;
    }
    int i_vlckey = qtWheelEventToVLCKey( e );
    var_SetInteger( p_intf->obj.libvlc, "key-pressed", i_vlckey );
    e->accept();
}

void MainInterface::closeEvent( QCloseEvent *e )
{
//  hide();
    if ( b_minimalView )
        setMinimalView( false );
    if( videoWidget )
        releaseVideoSlot( true );
    emit askToQuit(); /* ask THEDP to quit, so we have a unique method */
    /* Accept session quit. Otherwise we break the desktop mamager. */
    e->accept();
}

bool MainInterface::eventFilter( QObject *obj, QEvent *event )
{
    /* Alt+Tab: the Alt press was ours, its release is not */
    if( event->type() == QEvent::WindowDeactivate )
        b_altAlone = false;
    /* any other way back (tray, video on top) leaves the mini player too,
     * so View > Mini Player stays right */
    if( obj == this && event->type() == QEvent::Show && isMiniPlayerVisible() )
    {
        miniPlayer->hide();
        emit miniPlayerToggled( false );
    }
    if ( event->type() == MainInterface::ToolbarsNeedRebuild ) {
        event->accept();
        recreateToolbars();
        return true;
    } else {
        return QObject::eventFilter( obj, event );
    }
}

void MainInterface::toolBarConfUpdated()
{
    QApplication::postEvent( this, new QEvent( MainInterface::ToolbarsNeedRebuild ) );
}

void MainInterface::setInterfaceFullScreen( bool fs )
{
    titleBar->setFullScreen( fs, b_videoFullScreen );
    if( fs )
    {
        updateCustomFrame( true );
        setWindowState( windowState() | Qt::WindowFullScreen );
    }
    else
    {
        setWindowState( windowState() & ~Qt::WindowFullScreen );
        updateCustomFrame( false );
    }
}
void MainInterface::toggleInterfaceFullScreen()
{
    b_interfaceFullScreen = !b_interfaceFullScreen;
    if( !b_videoFullScreen )
        setInterfaceFullScreen( b_interfaceFullScreen );
    emit fullscreenInterfaceToggled( b_interfaceFullScreen );
}

void MainInterface::emitBoss()
{
    emit askBoss();
}
void MainInterface::setBoss()
{
    THEMIM->pause();
    if( sysTray )
    {
        hide();
    }
    else
    {
        showMinimized();
    }
}

void MainInterface::emitRaise()
{
    emit askRaise();
}
void MainInterface::setRaise()
{
    /* the mini player stands in for the hidden window (qt-auto-raise on the
     * next video, intf-show): raise it, do not bring the window back */
    if( isMiniPlayerVisible() )
    {
        miniPlayer->raise();
        miniPlayer->activateWindow();
        return;
    }

    /* If hidden, show it */
    if( isHidden() )
    {
        show();
    }
    else if( isMinimized() )
    {
        /* Minimized */
        showNormal();
    }

    activateWindow();
    raise();
}

void MainInterface::voutReleaseMouseEvents()
{
    if (videoWidget)
    {
        QPoint pos = QCursor::pos();
        QPoint localpos = videoWidget->mapFromGlobal(pos);
        int buttons = QApplication::mouseButtons();
        int i_button = 1;
        while (buttons != 0)
        {
            if ( (buttons & 1) != 0 )
            {
                QMouseEvent new_e( QEvent::MouseButtonRelease, localpos,
                                   (Qt::MouseButton)i_button, (Qt::MouseButton)i_button, Qt::NoModifier );
                QApplication::sendEvent(videoWidget, &new_e);
            }
            buttons >>= 1;
            i_button <<= 1;
        }

    }
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    if( p_intf->pf_show_dialog )
    {
        p_intf->pf_show_dialog( p_intf, INTF_DIALOG_POPUPMENU,
                                new_val.b_bool, NULL );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfShowCB: callback triggered by the intf-toggle-fscontrol libvlc variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->toggleFSC();

    /* Show event */
     return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfRaiseMainCB: callback triggered by the intf-show-main libvlc variable.
 *****************************************************************************/
static int IntfRaiseMainCB( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->emitRaise();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfBossCB: callback triggered by the intf-boss libvlc variable.
 *****************************************************************************/
static int IntfBossCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->emitBoss();

    return VLC_SUCCESS;
}
