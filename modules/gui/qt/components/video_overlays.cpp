/*****************************************************************************
 * video_overlays.cpp : paused badge, buffering ring and immersive bar
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
#include "input_manager.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_vout.h>

#include <QAbstractButton>
#include <QApplication>
#include <QPainter>
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

    const QEvent::Type WakeEvent = static_cast<QEvent::Type>( QEvent::registerEventType() );
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
ImmersiveBar::ImmersiveBar( intf_thread_t *_p_intf, QWidget *owner, QWidget *area,
                            bool advanced )
    : OverlayWindow( owner, TakeInput ), p_intf( _p_intf ),
      inputC( NULL ), controls( NULL ), wakePosted( 0 ), active( false ), asleep( false )
{
    setAccessibleName( qtr( "Playback controls" ) );
    const Theme *t = Theme::instance();
    layout = new QVBoxLayout( this );
    layout->setSpacing( 0 );
    layout->setContentsMargins( t->px( V::ImmersivePadX ), t->px( V::ImmersivePadTop ),
                                t->px( V::ImmersivePadX ), t->px( V::ImmersivePadBottom ) );
    rebuild( advanced );

    host = new OverlayHost( area, this );
    connect( host, &OverlayHost::changed, this, &ImmersiveBar::place );

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
}

void ImmersiveBar::rebuild( bool advanced )
{
    delete inputC;
    delete controls;
    inputC = new InputControlsWidget( p_intf, this );
    controls = new ControlsWidget( p_intf, advanced, this );
    controls->setGripVisible( false );
    layout->addWidget( inputC );
    layout->addWidget( controls );
    if( active )
        place();
}

/* Qt thread: voutListChanged is emitted by InputManager::UpdateVout */
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
            vlc_object_release( p_vout );
        }
    foreach( vout_thread_t *p_vout, set )
        if( !vouts.contains( p_vout ) )
        {
            vlc_object_hold( p_vout );
            var_AddCallback( p_vout, "mouse-moved", MouseMovedCB, this );
        }
    vouts = set;
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

bool ImmersiveBar::eventFilter( QObject *, QEvent *e )
{
    if( e->type() == QEvent::KeyPress )
        wake();
    return false;
}

void ImmersiveBar::setActive( bool on )
{
    if( on == active )
        return;
    active = on;
    asleep = false;
    if( on )
    {
        place();
        idle.start();
    }
    else
    {
        idle.stop();
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
        asleep = false;
        animate( true );
    }
}

/* Only a playing video hides it; never under the mouse or with one of its
 * menus open. A later mouse move, key or state change restarts the count. */
void ImmersiveBar::sleepIfIdle()
{
    InputManager *im = THEMIM->getIM();
    if( !active || asleep || im->playingStatus() != PLAYING_S || !im->hasVideo()
        || underMouse() || QApplication::activePopupWidget() )
        return;
    asleep = true;
    animate( false );
}

void ImmersiveBar::animate( bool in )
{
    const int ms = Theme::instance()->duration( Theme::ImmersiveFade );
    anim->stop();
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
