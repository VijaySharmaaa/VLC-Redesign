/*****************************************************************************
 * video_overlays.cpp : paused badge, buffering ring, immersive bar, header
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/video_overlays.hpp"
#include "components/controller.hpp"
#include "components/now_playing.hpp"
#include "components/library/library_widgets.hpp"
#include "input_manager.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_input.h>
#include <vlc_vout.h>

#ifdef _WIN32
# include <windows.h>
#endif

#include <QAbstractButton>
#include <QApplication>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QStyle>
#include <QWindow>
#include <QVariantAnimation>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QVBoxLayout>

namespace V = RedesignTokens::VideoOverlay;

namespace
{
    /* The badge itself: a real button, for the click and screen readers */
    class BadgeButton : public QAbstractButton
    {
    public:
        explicit BadgeButton( QWidget *parent ) : QAbstractButton( parent ), scale( 1. ) {}
        qreal scale;    /* the pop's, PopFromPercent .. 1 */

    protected:
        void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE
        {
            Theme *t = Theme::instance();
            QPainter p( this );
            p.setRenderHint( QPainter::Antialiasing );
            const QPointF c = QRectF( rect() ).center();
            p.translate( c );
            p.scale( scale, scale );
            p.translate( -c );
            p.setPen( t->ink( V::BadgeBorderAlpha ) );  /* a line width: not scaled */
            p.setBrush( t->color( Theme::PausedBadgeBg ) );
            p.drawEllipse( QRectF( rect() ).adjusted( .5, .5, -.5, -.5 ) );
            const int g = t->px( V::BadgeGlyph );
            t->icon( RedesignIcons::IconPlay, Theme::OnVideo, QSize( g, g ) )
                .paint( &p, QRect( rect().center() - QPoint( g / 2, g / 2 ), QSize( g, g ) ) );
        }
    };

    /* The header's round back button, white on the video in both themes */
    class HeaderBackButton : public QAbstractButton
    {
    public:
        explicit HeaderBackButton( QWidget *parent )
            : QAbstractButton( parent ), glyph( backIcon( Theme::OnVideo ) ) {}

    protected:
        void enterEvent( QEvent * ) Q_DECL_OVERRIDE { update(); }
        void leaveEvent( QEvent * ) Q_DECL_OVERRIDE { update(); }
        void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE
        {
            Theme *t = Theme::instance();
            QPainter p( this );
            p.setRenderHint( QPainter::Antialiasing );
            QColor fill = t->color( Theme::OnVideo );
            fill.setAlpha( underMouse() || isDown() ? V::HeaderBackHoverAlpha : V::HeaderBackAlpha );
            p.setPen( Qt::NoPen );
            p.setBrush( fill );
            p.drawEllipse( QRectF( rect() ) );
            if( hasFocus() )
            {
                const qreal inset = RedesignTokens::Size::FocusRing / 2.;
                p.setPen( QPen( t->color( Theme::Accent ), RedesignTokens::Size::FocusRing ) );
                p.setBrush( Qt::NoBrush );
                p.drawEllipse( QRectF( rect() ).adjusted( inset, inset, -inset, -inset ) );
            }
            const int g = t->px( V::HeaderBackGlyph );
            glyph.paint( &p, QRect( rect().center() - QPoint( g / 2, g / 2 ), QSize( g, g ) ) );
        }

    private:
        QIcon glyph;
    };

    const QEvent::Type WakeEvent = static_cast<QEvent::Type>( QEvent::registerEventType() );
}

/* shortcut: ignores crop and zoom (the picture is then larger than this),
 * upgrade if overlays must follow them */
QRect videoPictureRect( intf_thread_t *p_intf, const QRect &area )
{
    input_thread_t *input = THEMIM->getInput();
    if( !input || area.isEmpty() )
        return area;
    double dar = 0.;
    input_item_t *item = input_GetItem( input );
    vlc_mutex_lock( &item->lock );
    for( int i = 0; i < item->i_es && dar == 0.; i++ )
    {
        const video_format_t &v = item->es[i]->video;
        if( item->es[i]->i_cat == VIDEO_ES && v.i_visible_width && v.i_visible_height )
            dar = double( v.i_visible_width ) / v.i_visible_height
                * ( v.i_sar_num && v.i_sar_den ? double( v.i_sar_num ) / v.i_sar_den : 1. );
    }
    vlc_mutex_unlock( &item->lock );
    vout_thread_t *vout = input_GetVout( input );
    if( vout )
    {
        char *ar = var_GetString( vout, "aspect-ratio" );
        unsigned num, den;
        if( ar && sscanf( ar, "%u:%u", &num, &den ) == 2 && num && den )
            dar = double( num ) / den;
        free( ar );
        vlc_object_release( vout );
    }
    if( dar <= 0. )
        return area;
    QSize pic( qRound( area.height() * dar ), area.height() );
    if( pic.width() > area.width() )
        pic = QSize( area.width(), qRound( area.width() / dar ) );
    QRect r( QPoint(), pic );
    r.moveCenter( area.center() );
    return r;
}

/**********************************************************************
 * Paused badge
 **********************************************************************/
PausedBadge::PausedBadge( intf_thread_t *p_intf, QWidget *owner, QWidget *video )
    : OverlayWindow( owner, TakeInput ), im( THEMIM->getIM() ), wanted( false )
{
    BadgeButton *badge = new BadgeButton( this );
    button = badge;
    button->setFocusPolicy( Qt::NoFocus );
    button->setAccessibleName( qtr( "Play" ) );
    /* the play/pause hotkey, as the preferences store it (first binding) */
    char *key = var_InheritString( p_intf, "key-play-pause" );
    const QString keyName = qfu( key ).section( '\t', 0, 0 );
    free( key );
    button->setToolTip( keyName.isEmpty() ? qtr( "Play" )
                                          : qtr( "Play" ) + " (" + keyName + ")" );
    connect( button, &QAbstractButton::clicked, THEMIM, &MainInputManager::play );

    pop = new QVariantAnimation( this );
    pop->setStartValue( 0. );
    pop->setEndValue( 1. );
    pop->setEasingCurve( enterCurve() );
    connect( pop, &QVariantAnimation::valueChanged, this, [this, badge]( const QVariant &v ) {
        const qreal t = v.toReal();
        setWindowOpacity( t );
        badge->scale = ( V::PopFromPercent + ( 100 - V::PopFromPercent ) * t ) / 100.;
        badge->update();
    } );

    host = new OverlayHost( video, this );
    connect( host, &OverlayHost::changed, this, &PausedBadge::place );
    connect( im, &InputManager::playingStatusChanged, this, [this]() { refresh(); } );
    connect( im, &InputManager::voutChanged, this, [this]() { refresh(); } );
    refresh();
}

void PausedBadge::refresh()
{
    const bool now = im->playingStatus() == PAUSE_S && im->hasVideo();
    if( now == wanted )
        return;
    wanted = now;
    pop->stop();
    const int ms = Theme::instance()->duration( Theme::PausedBadgeFade );
    if( wanted && ms )
    {
        pop->setDuration( ms );
        pop->start();   /* from transparent: before place() shows it */
    }
    place();
}

void PausedBadge::place()
{
    if( !wanted || !host->targetShown() )
    {
        pop->stop();
        hide();
        return;
    }
    if( pop->state() != QAbstractAnimation::Running )
    {
        setWindowOpacity( 1. );
        static_cast<BadgeButton *>( button )->scale = 1.;
    }
    const int d = Theme::instance()->metric( Theme::PausedBadge );
    setGeometry( QRect( host->targetRect().center() - QPoint( d / 2, d / 2 ), QSize( d, d ) ) );
    button->setGeometry( rect() );
    show();
}

/**********************************************************************
 * Buffering
 **********************************************************************/
BufferingOverlay::BufferingOverlay( intf_thread_t *_p_intf, QWidget *owner, QWidget *area )
    : OverlayWindow( owner ), p_intf( _p_intf ), level( 0.f ), onPlayer( false )
{
    host = new OverlayHost( area, this );
    connect( host, &OverlayHost::changed, this, &BufferingOverlay::place );

    spin = new QVariantAnimation( this );
    spin->setStartValue( 0. );
    spin->setEndValue( 360. );
    spin->setLoopCount( -1 );
    connect( spin, &QVariantAnimation::valueChanged, this,
             static_cast<void (QWidget::*)()>( &QWidget::update ) );

    delay.setSingleShot( true );
    delay.setInterval( V::BufferingShowDelay );
    connect( &delay, &QTimer::timeout, this, &BufferingOverlay::place );

    InputManager *im = THEMIM->getIM();
    connect( im, &InputManager::cachingChanged, this,
             [this]( float f ) { level = f; refresh(); } );
    connect( im, &InputManager::playingStatusChanged, this, [this]() { refresh(); } );
}

void BufferingOverlay::setOnPlayer( bool b )
{
    onPlayer = b;
    refresh();
}

bool BufferingOverlay::buffering() const
{
    InputManager *im = THEMIM->getIM();
    const int status = im->playingStatus();
    return onPlayer && level > 0.f && level < 1.f && im->hasInput()
        && status != END_S && status != ERROR_S;
}

void BufferingOverlay::refresh()
{
    if( !buffering() )
    {
        delay.stop();
        url.clear();
        place();
        return;
    }
    if( url.isEmpty() )
    {
        input_item_t *item = input_GetItem( THEMIM->getInput() );
        char *uri = input_item_GetURI( item );
        url = qfu( uri );
        free( uri );
    }
    setAccessibleName( qtr( "Buffering… %1%" ).arg( qRound( level * 100 ) ) );
    if( isVisible() )
        update();
    else if( !delay.isActive() )
        delay.start();
}

void BufferingOverlay::place()
{
    if( !buffering() || delay.isActive() || !host->targetShown() )
    {
        spin->stop();
        hide();
        return;
    }
    setGeometry( host->targetRect() );
    show();
    const int ms = Theme::instance()->duration( Theme::Spinner );
    if( ms && spin->state() != QAbstractAnimation::Running )
    {
        spin->setDuration( ms );
        spin->start();
    }
}

void BufferingOverlay::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    p.fillRect( rect(), QColor::fromRgba( RedesignTokens::BufferingScrim ) );

    /* ring, title and URL in a centred column; text in the dark theme's
     * colours, as everything drawn on the video */
    const QFont titleFont = t->font( Theme::PanelTitle ), urlFont = t->font( Theme::Mono );
    const QFontMetrics titleFm( titleFont ), urlFm( urlFont );
    const int ring = t->px( V::Ring ), gap = t->px( V::BufferingGap );
    int y = ( height() - ring - 2 * gap - titleFm.height() - urlFm.height() ) / 2;

    const int w = t->px( V::RingWidth );
    const QRectF ringRect = QRectF( ( width() - ring ) / 2, y, ring, ring )
                                .adjusted( w / 2., w / 2., -w / 2., -w / 2. );
    QColor track = t->color( Theme::OnVideo );
    track.setAlpha( V::RingAlpha );
    p.setPen( QPen( track, w ) );
    p.drawEllipse( ringRect );
    /* the accent top quarter, turning clockwise (Qt's angles turn the other way) */
    const qreal turn = spin->state() == QAbstractAnimation::Running
                     ? spin->currentValue().toReal() : 0.;
    p.setPen( QPen( t->color( Theme::Accent ), w, Qt::SolidLine, Qt::FlatCap ) );
    p.drawArc( ringRect, qRound( ( 45. - turn ) * 16 ), 90 * 16 );
    y += ring + gap;

    p.setFont( titleFont );
    p.setPen( QColor::fromRgb( RedesignTokens::Dark.tx ) );
    p.drawText( QRect( 0, y, width(), titleFm.height() ), Qt::AlignCenter, accessibleName() );
    y += titleFm.height() + gap;

    p.setFont( urlFont );
    p.setPen( QColor::fromRgb( RedesignTokens::Dark.tx2 ) );
    const int room = width() - 2 * t->px( V::ImmersiveMargin );
    p.drawText( QRect( 0, y, width(), urlFm.height() ), Qt::AlignCenter,
                urlFm.elidedText( url, Qt::ElideMiddle, room ) );
}

/**********************************************************************
 * Immersive bar
 **********************************************************************/
ImmersiveBar::ImmersiveBar( intf_thread_t *_p_intf, QWidget *owner, QWidget *_area,
                            bool advanced )
    : OverlayWindow( owner, TakeInput ), p_intf( _p_intf ), area( _area ),
      inputC( NULL ), controls( NULL ), lift( 0 ), wakePosted( 0 ), active( false ), asleep( false )
{
    setAccessibleName( qtr( "Playback controls" ) );
    /* Tab moves the keys into it (takeFocus); a click still does not take
     * them from the window (nativeEvent), nor does a show (the overlay's
     * WA_ShowWithoutActivating) */
    setWindowFlags( windowFlags() & ~Qt::WindowDoesNotAcceptFocus );
    const Theme *t = Theme::instance();
    layout = new QVBoxLayout( this );
    layout->setSpacing( 0 );
    layout->setContentsMargins( t->px( V::ImmersivePadX ), t->px( V::ImmersivePadTop ),
                                t->px( V::ImmersivePadX ), t->px( V::ImmersivePadBottom ) );
    rebuild( advanced );

    host = new OverlayHost( area, this );
    connect( host, &OverlayHost::changed, this, &ImmersiveBar::place );
    /* every key of its window, before its buttons take them: they wake it */
    create();
    windowHandle()->installEventFilter( this );

    fade = new QPropertyAnimation( this, "windowOpacity", this );
    slide = new QPropertyAnimation( this, "pos", this );
    fade->setEasingCurve( enterCurve() );
    slide->setEasingCurve( enterCurve() );
    anim = new QParallelAnimationGroup( this );
    anim->addAnimation( fade );
    anim->addAnimation( slide );
    connect( anim, &QAbstractAnimation::finished, this, [this]() {
        if( asleep )
            hide();
    } );

    /* idle time is not motion: it stays when Windows animations are off */
    idle.setSingleShot( true );
    idle.setInterval( RedesignTokens::Motion::ImmersiveIdle );
    connect( &idle, &QTimer::timeout, this, &ImmersiveBar::sleepIfIdle );

    InputManager *im = THEMIM->getIM();
    /* direct: the list is released right after the signal */
    connect( im, &InputManager::voutListChanged, this, &ImmersiveBar::setVoutList,
             Qt::DirectConnection );
    connect( im, &InputManager::playingStatusChanged, this, &ImmersiveBar::wake );
    owner->installEventFilter( this );
    /* later: the update also destroys the popup menu, which may be the one
     * whose "Immersive Layout" item is creating this bar */
    QTimer::singleShot( 0, this, [this]() { THEMIM->requestVoutUpdate(); } );
}

ImmersiveBar::~ImmersiveBar()
{
    setVoutList( NULL, 0 );
    if( header )
        header->setAsleep( false );
}

void ImmersiveBar::setHeader( PlayerHeader *h )
{
    header = h;
    if( header )
        header->setAsleep( asleep );
}

void ImmersiveBar::setAsleep( bool s )
{
    asleep = s;
    if( header )
        header->setAsleep( s );
}

void ImmersiveBar::rebuild( bool advanced )
{
    delete inputC;
    delete controls;
    inputC = new InputControlsWidget( p_intf, this );
    controls = new ControlsWidget( p_intf, advanced, this );
    layout->addWidget( inputC );
    layout->addWidget( controls );
    if( active )
        place();
}

/* Qt thread: voutListChanged is emitted by InputManager::UpdateVout */
/* The lift is added to the vout's "sub-margin", never cached: the subtitle
 * position hotkeys (key-subpos-up/down) change the same variable meanwhile */
static void shiftSubMargin( vout_thread_t *p_vout, int by )
{
    if( by )
        var_SetInteger( p_vout, "sub-margin", var_GetInteger( p_vout, "sub-margin" ) + by );
}

void ImmersiveBar::setVoutList( vout_thread_t **pp_vout, int i_vout )
{
    QList<vout_thread_t *> set;
    for( int i = 0; i < i_vout; i++ )
        set << pp_vout[i];

    foreach( vout_thread_t *p_vout, vouts )
        if( !set.contains( p_vout ) )
        {
            /* returns once no callback runs: none posts to us after it */
            var_DelCallback( p_vout, "mouse-moved", MouseMovedCB, this );
            shiftSubMargin( p_vout, -lift );
            vlc_object_release( p_vout );
        }
    foreach( vout_thread_t *p_vout, set )
        if( !vouts.contains( p_vout ) )
        {
            vlc_object_hold( p_vout );
            var_AddCallback( p_vout, "mouse-moved", MouseMovedCB, this );
            shiftSubMargin( p_vout, lift );
        }
    vouts = set;
    if( i_vout )
        liftSubtitles();
}

/* While the bar is up, subtitles drawn under it move above it, as the
 * renders show: "sub-margin" on each vout, from the bottom of the picture.
 * Its unit is a px of the picture the vout blends subtitles into: the
 * video's own px, or the window's when the video is smaller than the
 * window (vout ThreadDisplayRenderPicture). */
void ImmersiveBar::liftSubtitles()
{
    int units = 0, px = 0;
    const QRect pic = videoPictureRect( p_intf, host->targetRect() );
    const int over = pic.bottom() - restingRect().top() + 1;   /* logical px */
    if( active && !asleep && host->targetShown() && over > 0 && !vouts.isEmpty() )
    {
        px = over;
        const qreal dpr = devicePixelRatioF();
        const QSize shown = pic.size() * dpr;
        input_thread_t *input = THEMIM->getInput();
        QSize video;
        if( input )
        {
            input_item_t *item = input_GetItem( input );
            vlc_mutex_lock( &item->lock );
            for( int i = 0; i < item->i_es && video.isEmpty(); i++ )
                if( item->es[i]->i_cat == VIDEO_ES )
                    video = QSize( item->es[i]->video.i_visible_width, item->es[i]->video.i_visible_height );
            vlc_mutex_unlock( &item->lock );
        }
        units = qRound( over * dpr );
        if( !video.isEmpty() && shown.height() > 0
         && qint64( video.width() ) * video.height() >= qint64( shown.width() ) * shown.height() )
            units = qRound( qreal( units ) * video.height() / shown.height() );
    }
    /* the toast keeps above the lifted lines (Toast::restingPos); the
     * audio page keeps its spectrum above the bar (immersiveInset, logical
     * px from the bottom; both come with a DynamicPropertyChange event) */
    area->setProperty( "subtitleLift", px );
    const bool up = active && !asleep && host->targetShown();
    area->setProperty( "immersiveInset",
                       up ? host->targetRect().bottom() + 1 - restingRect().top() : 0 );
    if( units == lift )
        return;
    foreach( vout_thread_t *p_vout, vouts )
        shiftSubMargin( p_vout, units - lift );
    lift = units;
}

/* vout thread: no widget access, only a posted event to the Qt thread */
int ImmersiveBar::MouseMovedCB( vlc_object_t *, const char *,
                                vlc_value_t, vlc_value_t, void *data )
{
    ImmersiveBar *bar = static_cast<ImmersiveBar *>( data );
    if( bar->wakePosted.testAndSetOrdered( 0, 1 ) )
        QApplication::postEvent( bar, new QEvent( WakeEvent ) );
    return VLC_SUCCESS;
}

void ImmersiveBar::customEvent( QEvent *e )
{
    if( e->type() != WakeEvent )
        return;
    wakePosted.storeRelease( 0 );
    wake();
}

bool ImmersiveBar::eventFilter( QObject *o, QEvent *e )
{
    if( e->type() != QEvent::KeyPress )
        return false;
    wake();
    const QKeyEvent *ke = static_cast<QKeyEvent *>( e );
    if( o == parentWidget() )
    {
        /* Tab while the window itself (or the player) has the keys: into
         * the bar, the deck's place in the tab order */
        const QWidget *fw = QApplication::focusWidget();
        return ke->key() == Qt::Key_Tab && ke->modifiers() == Qt::NoModifier
            && ( !fw || fw == parentWidget() || area->isAncestorOf( fw ) )
            && takeFocus();
    }
    if( ke->key() == Qt::Key_Escape )
    {
        giveFocusBack();
        return true;
    }
    return false;
}

/* Shows it and gives the keys to its first control */
bool ImmersiveBar::takeFocus()
{
    if( !active )
        return false;
    wake();
    QWidget *w = nextInFocusChain();
    while( w != this && !( w->isVisible() && w->isEnabled() && ( w->focusPolicy() & Qt::TabFocus ) ) )
        w = w->nextInFocusChain();
    if( w == this )
        return false;
    w->setFocus( Qt::TabFocusReason );
    activateWindow();
    return true;
}

void ImmersiveBar::setActive( bool on )
{
    if( on == active )
        return;
    active = on;
    setAsleep( false );
    if( on )
    {
        place();
        idle.start();
    }
    else
    {
        idle.stop();
        giveFocusBack();
        place();
    }
}

void ImmersiveBar::wake()
{
    if( !active )
        return;
    idle.start();
    if( asleep )
    {
        setAsleep( false );
        animate( true );
    }
}

/* Only a playing video hides it; never under the mouse or with one of its
 * menus open. A later mouse move, key or state change restarts the count. */
void ImmersiveBar::sleepIfIdle()
{
    InputManager *im = THEMIM->getIM();
    if( !active || asleep || im->playingStatus() != PLAYING_S || !im->hasVideo()
        || underMouse() || ( header && header->hovered() )
        || QApplication::activePopupWidget() )
        return;
    giveFocusBack();   /* idle: the keys go back with it */
    setAsleep( true );
    animate( false );
}

void ImmersiveBar::animate( bool in )
{
    const int ms = Theme::instance()->duration( Theme::ImmersiveFade );
    anim->stop();
    liftSubtitles();
    if( !ms || !host->targetShown() )
    {
        place();
        return;
    }
    const QPoint rest = restingRect().topLeft();
    const QPoint low = rest + QPoint( 0, Theme::instance()->px( V::ImmersiveOffset ) );
    if( in && !isVisible() )
    {
        setWindowOpacity( 0. );
        setGeometry( QRect( low, restingRect().size() ) );
        show();
    }
    fade->setStartValue( windowOpacity() );
    fade->setEndValue( in ? 1. : 0. );
    slide->setStartValue( pos() );
    slide->setEndValue( in ? rest : low );
    fade->setDuration( ms );
    slide->setDuration( ms );
    anim->start();
}

void ImmersiveBar::place()
{
    anim->stop();
    liftSubtitles();
    if( !active || asleep || !host->targetShown() )
    {
        hide();
        return;
    }
    setWindowOpacity( 1. );
    setGeometry( restingRect() );
    show();
}

QRect ImmersiveBar::restingRect() const
{
    const QRect r = host->targetRect();
    const int m = Theme::instance()->px( V::ImmersiveMargin );
    const int h = sizeHint().height();
    return QRect( r.left() + m, r.bottom() + 1 - m - h, r.width() - 2 * m, h );
}

/* shortcut: no backdrop blur and no drop shadow (a layered window would need
 * DWM acrylic, Windows 10+ only), upgrade with the Phase 6 Mica/acrylic work */
void ImmersiveBar::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    const int radius = t->metric( Theme::RadiusTile );
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( t->ink( V::ImmersiveBorderAlpha ) );
    p.setBrush( t->color( Theme::Glass ) );
    p.drawRoundedRect( QRectF( rect() ).adjusted( .5, .5, -.5, -.5 ), radius, radius );
}

/**********************************************************************
 * Player header
 **********************************************************************/
PlayerHeader::PlayerHeader( intf_thread_t *_p_intf, QWidget *owner, QWidget *_video,
                            NowPlayingInfo *_info )
    : OverlayWindow( owner ), p_intf( _p_intf ), info( _info ), video( _video ), active( false ), asleep( false )
{
    /* only the button takes the mouse: the strip leaves it to the video */
    backWindow = new OverlayWindow( owner, TakeInput );
    /* one of the player controls: Space on it plays or pauses (ButtonKeys) */
    backWindow->setObjectName( "playerHeader" );
    backWindow->setWindowFlags( backWindow->windowFlags() & ~Qt::WindowDoesNotAcceptFocus );
    back = new HeaderBackButton( backWindow );
    back->setFocusPolicy( Qt::TabFocus );
    back->setAccessibleName( qtr( "Go to library" ) );
    back->setToolTip( qtr( "Go to library" ) );
    /* its Tab, Shift+Tab and Esc, before the button takes them */
    backWindow->winId();   /* creates its window */
    backWindow->windowHandle()->installEventFilter( this );
    owner->installEventFilter( this );

    fade = new QVariantAnimation( this );
    fade->setEasingCurve( enterCurve() );
    connect( fade, &QVariantAnimation::valueChanged, this,
             [this]( const QVariant &v ) { setOpacity( v.toReal() ); } );
    connect( fade, &QAbstractAnimation::finished, this, [this]() {
        if( asleep )
            place();
    } );

    host = new OverlayHost( video, this );
    connect( host, &OverlayHost::changed, this, &PlayerHeader::place );
    connect( Theme::instance(), &Theme::changed, this, &PlayerHeader::place );
    connect( info, &NowPlayingInfo::changed, this, &PlayerHeader::refresh );
    refresh();
}

PlayerHeader::~PlayerHeader()
{
    delete backWindow;
}

void PlayerHeader::refresh()
{
    input_thread_t *input = THEMIM->getInput();
    meta = input ? mediaMetaLine( input_GetItem( input ) ) : QString();
    setAccessibleName( info->title );
    update();
}

void PlayerHeader::setActive( bool on )
{
    if( on == active )
        return;
    active = on;
    place();
}

void PlayerHeader::setAsleep( bool s )
{
    if( s == asleep )
        return;
    asleep = s;
    const int ms = Theme::instance()->duration( Theme::ImmersiveFade );
    if( !ms || !active || !host->targetShown() )
    {
        place();
        return;
    }
    if( !asleep && !isVisible() )
    {
        place();
        setOpacity( 0. );
    }
    fade->stop();
    fade->setDuration( ms );
    fade->setStartValue( windowOpacity() );
    fade->setEndValue( asleep ? 0. : 1. );
    fade->start();
}

bool PlayerHeader::hovered() const
{
    return backWindow->isVisible() && back->underMouse();
}

void PlayerHeader::setOpacity( qreal o )
{
    setWindowOpacity( o );
    backWindow->setWindowOpacity( o );
}

void PlayerHeader::place()
{
    fade->stop();
    if( !active || asleep || !host->targetShown() )
    {
        backWindow->giveFocusBack();
        backWindow->hide();
        hide();
        return;
    }
    const Theme *t = Theme::instance();
    const QRect r = host->targetRect();
    const int h = qMin( r.height(), 2 * t->px( V::HeaderPadY ) + rowHeight() );
    setGeometry( QRect( r.topLeft(), QSize( r.width(), h ) ) );
    const QRect b = backRect( r.width() );
    backWindow->setGeometry( b.translated( r.topLeft() ) );
    back->setGeometry( QRect( QPoint(), b.size() ) );
    setOpacity( 1. );
    if( !isVisible() )
    {
        show();
        backWindow->show();
        raiseDrawers();
    }
}

/* The XS drawers (playlist, Effects sheet) are tool windows of the same
 * owner over the video: a header shown after them stays below them */
void PlayerHeader::raiseDrawers()
{
    foreach( QWidget *w, parentWidget()->findChildren<QWidget *>( QString(),
                                                                 Qt::FindDirectChildrenOnly ) )
    {
        if( !w->isWindow() || !w->isVisible()
         || !( w->inherits( "PlaylistPanel" ) || w->inherits( "ExtendedDialog" ) ) )
            continue;
#ifdef _WIN32
        /* not raise(): it would take the keys from the main window */
        SetWindowPos( reinterpret_cast<HWND>( w->winId() ), HWND_TOP, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
#else
        w->raise();
#endif
    }
}

QFont PlayerHeader::titleFont() const
{
    /* PanelTitle's semibold, at the header's size */
    QFont f = Theme::instance()->font( Theme::PanelTitle );
    f.setPointSizeF( f.pointSizeF() * V::HeaderTitle / RedesignTokens::Type::PanelTitle );
    return f;
}

int PlayerHeader::rowHeight() const
{
    const QFontMetrics title( titleFont() ), meta( Theme::instance()->font( Theme::Meta ) );
    return qMax( Theme::instance()->px( V::HeaderBack ), title.height() + meta.height() );
}

QRect PlayerHeader::backRect( int width ) const
{
    const Theme *t = Theme::instance();
    const int b = t->px( V::HeaderBack );
    const QRect r( t->px( V::HeaderPadX ), t->px( V::HeaderPadY ) + ( rowHeight() - b ) / 2, b, b );
    return QStyle::visualRect( layoutDirection(), QRect( 0, 0, width, b ), r );
}

bool PlayerHeader::eventFilter( QObject *o, QEvent *e )
{
    if( e->type() != QEvent::KeyPress )
        return false;
    const QKeyEvent *ke = static_cast<QKeyEvent *>( e );
    if( o == parentWidget() )
    {
        /* Shift+Tab while the window itself or the video has the keys: to
         * the back button, before the deck in the tab order */
        const QWidget *fw = QApplication::focusWidget();
        if( ke->key() != Qt::Key_Backtab || !backWindow->isVisible() || asleep
         || ( fw && fw != o && fw != video && !video->isAncestorOf( fw ) ) )
            return false;
        back->setFocus( Qt::BacktabFocusReason );
        backWindow->activateWindow();
        return true;
    }
    /* in the back button's window: Tab goes on to the deck, Shift+Tab and
     * Esc give the keys back */
    if( ke->key() == Qt::Key_Tab && ke->modifiers() == Qt::NoModifier )
    {
        backWindow->giveFocusBack();
        QKeyEvent tab( QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier );
        QApplication::sendEvent( parentWidget(), &tab );
        return true;
    }
    if( ke->key() == Qt::Key_Backtab || ke->key() == Qt::Key_Escape )
    {
        backWindow->giveFocusBack();
        return true;
    }
    return false;
}

void PlayerHeader::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    QLinearGradient scrim( 0, 0, 0, height() );
    scrim.setColorAt( 0, QColor::fromRgba( RedesignTokens::HeaderScrim ) );
    scrim.setColorAt( 1, Qt::transparent );
    p.fillRect( rect(), scrim );

    /* title and meta beside the button; in the dark theme's colours, as
     * everything drawn on the video */
    const QFont title = titleFont(), metaFont = t->font( Theme::Meta );
    const QFontMetrics titleFm( title ), metaFm( metaFont );
    const int x = t->px( V::HeaderPadX ) + t->px( V::HeaderBack ) + t->px( V::HeaderGap );
    const int w = width() - x - t->px( V::HeaderPadX );
    if( w <= 0 )
        return;
    const int block = titleFm.height() + ( meta.isEmpty() ? 0 : metaFm.height() );
    int y = t->px( V::HeaderPadY ) + ( rowHeight() - block ) / 2;
    const Qt::LayoutDirection dir = layoutDirection();
    const Qt::Alignment align = QStyle::visualAlignment( dir, Qt::AlignLeft | Qt::AlignVCenter );
    p.setLayoutDirection( dir );

    p.setFont( title );
    p.setPen( QColor::fromRgb( RedesignTokens::Dark.tx ) );
    p.drawText( QStyle::visualRect( dir, rect(), QRect( x, y, w, titleFm.height() ) ), align,
                titleFm.elidedText( info->title, Qt::ElideRight, w ) );
    y += titleFm.height();
    p.setFont( metaFont );
    p.setPen( QColor::fromRgb( RedesignTokens::Dark.tx2 ) );
    p.drawText( QStyle::visualRect( dir, rect(), QRect( x, y, w, metaFm.height() ) ), align,
                metaFm.elidedText( meta, Qt::ElideRight, w ) );
}
