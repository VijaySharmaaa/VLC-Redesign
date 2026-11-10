/*****************************************************************************
 * input_slider.cpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ludovic Fauvet <etix@videolan.org>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include "util/input_slider.hpp"
#include "util/timetooltip.hpp"
#include "adapters/seekpoints.hpp"
#include "input_manager.hpp"
#include "imagehelper.hpp"
#include "customwidgets.hpp"
#include "util/theme.hpp"
#include <vlc_actions.h>

#include <QPaintEvent>
#include <QPainter>
#include <QBitmap>
#include <QStyleOptionSlider>
#include <QLinearGradient>
#include <QTimer>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QSize>
#include <QPalette>
#include <QColor>
#include <QPoint>
#include <QPropertyAnimation>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QSequentialAnimationGroup>

namespace {
    int const MIN_SLIDER_VALUE = 0;
    int const MAX_SLIDER_VALUE = 10000;

    int const CHAPTER_SPOT_SIZE = 3;
}

SeekSlider::SeekSlider( intf_thread_t *p_intf, Qt::Orientation q, QWidget *_parent, bool _static )
          : QSlider( q, _parent ), p_intf( p_intf ), b_classic( _static ), animLoading( NULL )
{
    isSliding = false;
    isJumping = false;
    f_buffering = 0.0;
    mLoading = 0.0;
    chapters = NULL;
    mHandleLength = -1;
    b_seekable = true;
    alternativeStyle = NULL;

    /* Timer used to fire intermediate updatePos() when sliding */
    seekLimitTimer = new QTimer( this );
    seekLimitTimer->setSingleShot( true );

    /* Tooltip bubble */
    mTimeTooltip = new TimeTooltip( NULL );
    mTimeTooltip->setMouseTracking( true );

    /* Properties */
    setRange( MIN_SLIDER_VALUE, MAX_SLIDER_VALUE );
    setSingleStep( 2 );
    setPageStep( 10 );
    setMouseTracking( true );
    setTracking( true );
    setFocusPolicy( Qt::NoFocus );
    setAccessibleName( qtr( "Seek" ) );

    /* Use the new/classic style. High enough for the hovered knob and its
     * halo (DESIGN_SPEC 3); the style's handle length follows the height. */
    const Theme *t = Theme::instance();
    setMinimumHeight( t->metric( Theme::SeekKnobHover ) + 2 * t->px( RedesignTokens::Deck::KnobHalo ) );
    if( !b_classic )
    {
        alternativeStyle = new SeekStyle;
        setStyle( alternativeStyle );
    }

    /* Init to 0 */
    setPosition( -1.0, 0, 0 );

    /* durations are set in startAnimLoading(), where reduced motion is read */
    QPropertyAnimation *animLoadingIn = new QPropertyAnimation( this, "loadingProperty", this );
    animLoadingIn->setStartValue( 0.0 );
    animLoadingIn->setEndValue( 1.0 );
    animLoadingIn->setEasingCurve( QEasingCurve::OutBounce );
    QPropertyAnimation *animLoadingOut = new QPropertyAnimation( this, "loadingProperty", this );
    animLoadingOut->setStartValue( 1.0 );
    animLoadingOut->setEndValue( 0.0 );
    animLoadingOut->setEasingCurve( QEasingCurve::OutBounce );

    animLoading = new QSequentialAnimationGroup( this );
    animLoading->addAnimation( animLoadingIn );
    animLoading->addAnimation( animLoadingOut );
    animLoading->setLoopCount( -1 );
    /* Windows animations turned off while it bounces: it stops at the next turn */
    connect( animLoading, &QAbstractAnimation::currentLoopChanged, this, [this]() {
        if( Theme::instance()->duration( Theme::SeekLoading ) == 0 )
        {
            animLoading->stop();
            mLoading = 0.0;
            update();
        }
    } );

    startAnimLoadingTimer = new QTimer( this );
    startAnimLoadingTimer->setSingleShot( true );
    startAnimLoadingTimer->setInterval( RedesignTokens::Motion::SeekLoadingDelay );

    connect( MainInputManager::getInstance(), &MainInputManager::inputChanged, this, &SeekSlider::inputUpdated );
    connect( this, &SeekSlider::sliderMoved, this, &SeekSlider::startSeekTimer );
    connect( seekLimitTimer, &QTimer::timeout, this, &SeekSlider::updatePos );
    connect( startAnimLoadingTimer, &QTimer::timeout, this, &SeekSlider::startAnimLoading );
    mTimeTooltip->installEventFilter( this );

    connect(&wheelEventConverter, &WheelToVLCConverter::vlcWheelKey, this, [this](int vlcButton){
        /* idle or live: no length to step through */
        if( inputLength <= 0 )
            return;
        vlc_tick_t i_size = var_InheritInteger( this->p_intf->obj.libvlc, "short-jump-size" );
        int i_mode = var_InheritInteger( this->p_intf->obj.libvlc, "hotkeys-x-wheel-mode" );

        //ignore modifiers
        switch (vlcButton & 0x00FF0000) {
        case KEY_MOUSEWHEELDOWN:
        case KEY_MOUSEWHEELLEFT:
            if (i_mode != 3)
                i_size = - i_size;
            break;
        case KEY_MOUSEWHEELUP:
        case KEY_MOUSEWHEELRIGHT:
            if (i_mode == 3)
                i_size = - i_size;
            break;
        default:
            break;
        }

        float posOffset = static_cast<float>( i_size ) / static_cast<float>( inputLength );
        setValue( value() + posOffset * maximum() );
        emit sliderDragged( value() / static_cast<float>( maximum() ) );
    });
}

SeekSlider::~SeekSlider()
{
    delete chapters;
    if ( alternativeStyle )
        delete alternativeStyle;
    delete mTimeTooltip;
}

/***
 * \brief Sets the chapters seekpoints adapter
 *
 * \params SeekPoints initilized with current intf thread
***/
void SeekSlider::setChapters( SeekPoints *chapters_ )
{
    delete chapters;
    chapters = chapters_;
    chapters->setParent( this );
}

/***
 * \brief Main public method, superseeding setValue. Disabling the slider when neeeded
 *
 * \param pos Position, between 0 and 1. -1 disables the slider
 * \param time Elapsed time, in microseconds
 * \param legnth Duration time.
 ***/
void SeekSlider::setPosition( float pos, int64_t time, int length )
{
    /* screen readers: the time, not the raw 0..10000 value (only on a new
     * second, each change is an accessibility event) */
    const QString where = pos < 0.f ? QString()
        : length > 0 ? qtr( "%1 of %2" ).arg( timeString( time / CLOCK_FREQ ), timeString( length ) )
                     : timeString( time / CLOCK_FREQ );
    if( where != accessibleDescription() )
        setAccessibleDescription( where );

    if( pos == -1.0  || ! b_seekable )
    {
        setEnabled( false );
        mTimeTooltip->hide();
        isSliding = false;
        setValue( 0 );
        return;
    }
    else
        setEnabled( true );

    if( !isSliding )
    {
        setValue( pos * static_cast<float>( maximum() ) );
        if ( animLoading != NULL && pos >= 0.0f && animLoading->state() != QAbstractAnimation::Stopped )
        {
            animLoading->stop();
            mLoading = 0.0f;
        }

    }

    inputLength = length;
}

void SeekSlider::startSeekTimer()
{
    /* Only fire one update, when sliding, every 150ms */
    if( isSliding && !seekLimitTimer->isActive() )
        seekLimitTimer->start( 150 );
}

void SeekSlider::updatePos()
{
    float f_pos = value() / static_cast<float>( maximum() );
    emit sliderDragged( f_pos ); /* Send new position to VLC's core */
}

void SeekSlider::updateBuffering( float f_buffering_ )
{
    if ( f_buffering_ < f_buffering )
        bufferingStart = QTime::currentTime();
    f_buffering = f_buffering_;
    if ( f_buffering > 0.0 || isEnabled() ) {
        animLoading->stop();
        startAnimLoadingTimer->stop();
        mLoading = 0.0;
    }
    repaint();
}

void SeekSlider::inputUpdated( bool b_has_input )
{
    if ( b_has_input == false ) {
        animLoading->stop();
        startAnimLoadingTimer->stop();
        mLoading = 0.0;
        repaint();
    }
    else if ( f_buffering == 0.0 && !isEnabled() )
        startAnimLoadingTimer->start();
}

void SeekSlider::processReleasedButton()
{
    if ( !isSliding && !isJumping ) return;
    isSliding = false;
    bool b_seekPending = seekLimitTimer->isActive();
    seekLimitTimer->stop(); /* We're not sliding anymore: only last seek on release */
    if ( isJumping )
    {
        isJumping = false;
        return;
    }
    if( b_seekPending && isEnabled() )
        updatePos();
}

void SeekSlider::mouseReleaseEvent( QMouseEvent *event )
{
    if ( event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton )
    {
        QSlider::mouseReleaseEvent( event );
        return;
    }
    event->accept();
    processReleasedButton();
}

void SeekSlider::mousePressEvent( QMouseEvent* event )
{
    /* Right-click */
    if ( !isEnabled() ||
         ( event->button() != Qt::LeftButton && event->button() != Qt::MiddleButton )
       )
    {
        QSlider::mousePressEvent( event );
        return;
    }

    isJumping = false;
    /* handle chapter clicks */
    int i_width = size().width();
    if ( chapters && inputLength && i_width)
    {
        if ( orientation() == Qt::Horizontal ) /* TODO: vertical */
        {
             /* only on chapters zone */
            if ( event->y() < CHAPTER_SPOT_SIZE ||
                 event->y() > ( size().height() - CHAPTER_SPOT_SIZE ) )
            {
                QList<SeekPoint> points = chapters->getPoints();
                int i_selected = -1;
                bool b_startsnonzero = false; /* as we always starts at 1 */
                if ( points.count() > 0 ) /* do we need an extra offset ? */
                    b_startsnonzero = ( points.at(0).time > 0 );
                int i_min_diff = i_width + 1;
                for( int i = 0 ; i < points.count() ; i++ )
                {
                    int x = points.at(i).time / 1000000.0 / inputLength * i_width;
                    int diff_x = abs( x - event->x() );
                    if ( diff_x < i_min_diff )
                    {
                        i_min_diff = diff_x;
                        i_selected = i + ( ( b_startsnonzero )? 1 : 0 );
                    } else break;
                }
                if ( i_selected && i_min_diff < 4 ) // max 4px around mark
                {
                    chapters->jumpTo( i_selected );
                    event->accept();
                    isJumping = true;
                    return;
                }
            }
        }
    }

    isSliding = true ;

    setValue( getValueFromXPos( event->x() ) );
    emit sliderMoved( value() );
    event->accept();
}

void SeekSlider::mouseMoveEvent( QMouseEvent *event )
{
    if ( ! ( event->buttons() & ( Qt::LeftButton | Qt::MiddleButton ) ) )
    {
        /* Handle button release when mouserelease has been hijacked by popup */
        processReleasedButton();
    }

    if ( !isEnabled() ) return event->accept();

    if( isSliding )
    {
        setValue( getValueFromXPos( event->x() ) );
        emit sliderMoved( value() );
    }

    /* Tooltip */
    if ( inputLength > 0 )
    {
        int margin = handleLength();
        int posX = qMax( rect().left() + margin, qMin( rect().right() - margin, event->x() ) );

        QString chapterLabel;

        if ( orientation() == Qt::Horizontal ) /* TODO: vertical */
        {
            QList<SeekPoint> points = chapters->getPoints();
            int i_selected = -1;
            for( int i = 0 ; i < points.count() ; i++ )
            {
                int x = margin + points.at(i).time / 1000000.0 / inputLength * (size().width() - 2*margin);
                if ( event->x() >= x )
                    i_selected = i;
            }
            if ( i_selected >= 0 && i_selected < points.size() )
            {
                chapterLabel = points.at( i_selected ).name;
            }
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const auto pos = event->globalPosition();
#else
        const auto pos = event->globalPos();
#endif
        QPoint target( pos.x() - ( event->x() - posX ),
                QWidget::mapToGlobal( QPoint( 0, 0 ) ).y() );
        if( likely( size().width() > handleLength() ) ) {
            mTimeTooltip->setTip( target, timeString( getValuePercentageFromXPos( event->x() ) * inputLength ),
                                  chapterLabel );
        }
    }
    event->accept();
}

void SeekSlider::wheelEvent( QWheelEvent *event )
{
    /* Don't do anything if we are for somehow reason sliding */
    if( !isSliding && isEnabled() )
        wheelEventConverter.wheelEvent(event);
    event->accept();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SeekSlider::enterEvent( QEnterEvent * )
#else
void SeekSlider::enterEvent( QEvent * )
#endif
{
    update(); /* the track and the knob grow on hover */
    /* Don't show the tooltip if the slider is disabled or a menu is open */
    if( isEnabled() && inputLength > 0 && !qApp->activePopupWidget() )
        mTimeTooltip->show();
}

void SeekSlider::leaveEvent( QEvent * )
{
    update();
    /* Hide the tooltip
       - if the mouse leave the slider rect (Note: it can still be
         over the tooltip!)
       - if another window is on the way of the cursor */
    if( !rect().contains( mapFromGlobal( QCursor::pos() ) ) ||
      ( !isActiveWindow() && !mTimeTooltip->isActiveWindow() ) )
    {
        mTimeTooltip->hide();
    }
}

void SeekSlider::paintEvent( QPaintEvent *ev )
{
    if ( alternativeStyle )
    {
        SeekStyle::SeekStyleOption option;
        option.initFrom( this );
        if ( QTime::currentTime() > bufferingStart.addSecs( 1 ) )
            option.buffering = f_buffering;
        else
            option.buffering = 0.0;
        option.length = inputLength;
        option.animationloading = mLoading;
        option.sliderPosition = sliderPosition();
        option.sliderValue = value();
        option.maximum = maximum();
        option.minimum = minimum();
        if ( chapters ) foreach( const SeekPoint &point, chapters->getPoints() )
            option.points << point.time;
        QPainter painter( this );
        /* Not style(): with an application stylesheet that is a QStyleSheetStyle,
         * which forwards a copy sliced to QStyleOptionSlider and loses our fields. */
        alternativeStyle->drawComplexControl( QStyle::CC_Slider, &option, &painter, this );
    }
    else
        QSlider::paintEvent( ev );
}

void SeekSlider::hideEvent( QHideEvent * )
{
    mTimeTooltip->hide();
}

bool SeekSlider::eventFilter( QObject *obj, QEvent *event )
{
    if( obj == mTimeTooltip )
    {
        if( event->type() == QEvent::MouseMove )
        {
            QMouseEvent* mev = static_cast<QMouseEvent*>( event );

            if( rect().contains( mapFromGlobal( mev->globalPos() ) ) )
                return false;
        }

        if( event->type() == QEvent::Leave ||
            event->type() == QEvent::MouseMove )
        {
            mTimeTooltip->hide();
        }

        return false;
    }

    return QSlider::eventFilter( obj, event );
}

QSize SeekSlider::sizeHint() const
{
    if ( b_classic )
        return QSlider::sizeHint();
    return ( orientation() == Qt::Horizontal ) ? QSize( 100, minimumHeight() )
                                               : QSize( minimumHeight(), 100 );
}

qreal SeekSlider::loading() const
{
    return mLoading;
}

void SeekSlider::setLoading(qreal loading)
{
    mLoading = loading;
    /* Request a new paintevent */
    update();
}

inline int SeekSlider::handleLength()
{
    if ( mHandleLength > 0 )
        return mHandleLength;

    /* Ask for the length of the handle to the underlying style */
    QStyleOptionSlider option;
    initStyleOption( &option );
    mHandleLength = style()->pixelMetric( QStyle::PM_SliderLength, &option );
    return mHandleLength;
}

inline int SeekSlider::getValueFromXPos( int posX )
{
    return QStyle::sliderValueFromPosition(
        minimum(), maximum(),
        posX    - handleLength() / 2,
        width() - handleLength(),
        false
    );
}

inline float SeekSlider::getValuePercentageFromXPos( int posX )
{
    return getValueFromXPos( posX ) / static_cast<float>( maximum() );
}

void SeekSlider::startAnimLoading()
{
    /* reduced motion: an endless bounce is motion, the disabled bar says enough */
    const int ms = Theme::instance()->duration( Theme::SeekLoading );
    if( ms == 0 )
        return;
    for( int i = 0; i < animLoading->animationCount(); i++ )
        static_cast<QPropertyAnimation *>( animLoading->animationAt( i ) )->setDuration( ms );
    animLoading->start();
}

/* This work is derived from Amarok's work under GPLv2+
    - Mark Kretschmann
    - Gábor Lehel
   */
#define SOUNDMIN  0   // %

/* DESIGN_SPEC 3: a flat 140 px track, 4 px high, a 1 px tick at 100 %, the
 * fill in the accent and in the boost colour above 100 %; the level as
 * text on the right. The maximum is qt-max-volume. */
SoundSlider::SoundSlider( QWidget *_parent, float _i_step,
                          char *psz_colors, int max )
                        : QAbstractSlider( _parent )
{
    f_step = (float)(_i_step * 10000)
           / (float)((max - SOUNDMIN) * AOUT_VOLUME_DEFAULT);
    setRange( SOUNDMIN, max);
    setSingleStep( qMax( 1, qRound( f_step ) ) );
    setMouseTracking( true );
    isSliding = false;
    b_mouseOutside = true;
    b_isMuted = false;

    const Theme *t = Theme::instance();
    setFixedSize( t->metric( Theme::VolumeW ) + t->px( RedesignTokens::Deck::VolumeLabelW ),
                  t->px( RedesignTokens::Deck::VolumeH ) );

    /* A qt-slider-colours the user changed wins over the theme's accent */
    module_config_t *p_colors = config_FindConfig( "qt-slider-colours" );
    if( psz_colors && !( p_colors && p_colors->orig.psz
                         && !strcmp( psz_colors, p_colors->orig.psz ) ) )
    {
        colorList = qfu( psz_colors ).split( ";" );
        /* Fill with 255 if the list is too short */
        while( colorList.count() < 12 )
            colorList.append( "255" );
    }
    free( psz_colors );

    connect( Theme::instance(), &Theme::changed, this,
             static_cast<void (QWidget::*)()>( &QWidget::update ) );

    connect(&wheelEventConverter, &WheelToVLCConverter::vlcWheelKey, this, [this](int vlcButton){
        int newvalue = 0;
        //ignore modifiers
        switch (vlcButton & 0x00FF0000) {
        case KEY_MOUSEWHEELDOWN:
        case KEY_MOUSEWHEELLEFT:
            newvalue = value() - f_step;
            break;
        case KEY_MOUSEWHEELUP:
        case KEY_MOUSEWHEELRIGHT:
            newvalue = value() + f_step;
            break;
        default:
            return;
        }

        setValue( __MIN( __MAX( minimum(), newvalue ), maximum() ) );
        emit sliderMoved( value() );
    });
}

QRect SoundSlider::trackRect() const
{
    const int h = Theme::instance()->metric( Theme::SeekTrack );
    return QRect( 0, ( height() - h ) / 2, Theme::instance()->metric( Theme::VolumeW ), h );
}

QBrush SoundSlider::fillBrush() const
{
    const Theme *theme = Theme::instance();
    if( colorList.isEmpty() )
    {
        if( b_isMuted )
            return theme->ink( RedesignTokens::InkAlpha::ToggleOff );
        if( value() <= 100 )
            return theme->color( Theme::Accent );
        /* DESIGN_SPEC 3: only the part above 100 % (the tick) is the boost */
        const QRect track = trackRect();
        QLinearGradient gradient( track.left(), 0, track.left() + track.width(), 0 );
        const qreal tick = 100.0 / maximum();
        gradient.setColorAt( tick, theme->color( Theme::Accent ) );
        gradient.setColorAt( qMin( 1.0, tick + 1.0 / track.width() ), theme->color( Theme::VolumeBoost ) );
        return gradient;
    }

    /* qt-slider-colours: four stops, the middle pair around 100 % */
    const QRect track = trackRect();
    QLinearGradient gradient( track.left(), 0, track.right(), 0 );
    const qreal f_mid_point = 100.0 / maximum();
    const qreal stops[] = { 0.0, qBound( 0.0, f_mid_point - 0.05, 1.0 ),
                            qBound( 0.0, f_mid_point + 0.05, 1.0 ), 1.0 };
#define c(i) colorList.at(i).toInt()
    for( int i = 0; i < 4; i++ )
    {
        QColor color( c( 3 * i ), c( 3 * i + 1 ), c( 3 * i + 2 ) );
        if( b_isMuted )  /* desaturated */
            color.setHsvF( color.hueF(), 0.2, 0.5, 1.0 );
        gradient.setColorAt( stops[i], color );
    }
#undef c
    return gradient;
}

void SoundSlider::wheelEvent( QWheelEvent *event )
{
    wheelEventConverter.wheelEvent(event);
    event->accept();
    emit sliderReleased();
}

void SoundSlider::mousePressEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        /* We enter the sliding mode */
        isSliding = true;
        i_oldvalue = value();
        emit sliderPressed();
        changeValue( event->x() );
        emit sliderMoved( value() );
    }
}

void SoundSlider::processReleasedButton()
{
    if( !b_mouseOutside && value() != i_oldvalue )
    {
        emit sliderReleased();
        setValue( value() );
        emit sliderMoved( value() );
    }
    isSliding = false;
    b_mouseOutside = false;
}

void SoundSlider::mouseReleaseEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
        processReleasedButton();
}

void SoundSlider::mouseMoveEvent( QMouseEvent *event )
{
    /* handle mouserelease hijacking */
    if ( isSliding && ( event->buttons() & ~Qt::RightButton ) == Qt::NoButton )
        processReleasedButton();

    const QRect track = trackRect();
    if( isSliding )
    {
        /* dragging far away from the track restores the old level */
        QRect rect( track.left() - 15, -1, track.width() + 15 * 2, width() + 5 );
        if( !rect.contains( event->pos() ) )
        { /* We are outside */
            if ( !b_mouseOutside )
                setValue( i_oldvalue );
            b_mouseOutside = true;
        }
        else
        { /* We are inside */
            b_mouseOutside = false;
            changeValue( event->x() );
            emit sliderMoved( value() );
        }
    }
    else
    {
        int i = ( ( event->x() - track.left() ) * maximum() ) / track.width();
        i = __MIN( __MAX( 0, i ), maximum() );
        setToolTip( QString("%1  %" ).arg( i ) );
    }
}

void SoundSlider::changeValue( int x )
{
    const QRect track = trackRect();
    setValue( ( ( x - track.left() ) * maximum() + track.width() / 2 ) / track.width() );
}

void SoundSlider::setMuted( bool m )
{
    b_isMuted = m;
    update();
}

void SoundSlider::paintEvent( QPaintEvent *e )
{
    const Theme *theme = Theme::instance();
    const QRect track = trackRect();
    const qreal radius = track.height() / 2.;
    QPainter painter( this );
    painter.setRenderHint( QPainter::Antialiasing );
    painter.setPen( Qt::NoPen );

    painter.setBrush( theme->ink( RedesignTokens::InkAlpha::Track ) );
    painter.drawRoundedRect( track, radius, radius );
    /* + max/2 rounds to the nearest pixel for any qt-max-volume */
    const int fill = ( track.width() * value() + maximum() / 2 ) / maximum();
    if( fill > 0 )
    {
        painter.setBrush( fillBrush() );
        painter.drawRoundedRect( QRect( track.left(), track.top(), fill, track.height() ), radius, radius );
    }
    if( maximum() > 100 )
    {
        const int tickH = theme->px( RedesignTokens::Deck::VolumeTickH );
        painter.fillRect( QRect( track.left() + track.width() * 100 / maximum(), ( height() - tickH ) / 2,
                                 RedesignTokens::Size::Hairline, tickH ),
                          theme->ink( RedesignTokens::InkAlpha::VolumeTick ) );
    }
    if( hasFocus() )
    {
        /* the accent is 2.4:1 on light surfaces: rings take the accent text there */
        const qreal inset = RedesignTokens::Size::FocusRing / 2.;
        painter.setPen( QPen( theme->color( theme->isDark() ? Theme::Accent : Theme::AccentText ),
                              RedesignTokens::Size::FocusRing ) );
        painter.setBrush( Qt::NoBrush );
        painter.drawRoundedRect( QRectF( rect() ).adjusted( inset, inset, -inset, -inset ),
                                 theme->metric( Theme::RadiusSm ), theme->metric( Theme::RadiusSm ) );
    }

    painter.setPen( theme->color( Theme::Text2 ) );
    painter.setFont( theme->font( Theme::Meta ) );
    painter.drawText( QRect( track.right() + 1, 0, width() - track.right() - 1, height() ),
                      Qt::AlignRight | Qt::AlignVCenter, QString::number( value() ) + '%' );
    e->accept();
}
