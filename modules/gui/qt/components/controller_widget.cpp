/*****************************************************************************
 * controller_widget.cpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include "controller_widget.hpp"
#include "controller.hpp"

#include "input_manager.hpp"         /* Get notification of Volume Change */
#include "util/input_slider.hpp"     /* SoundSlider */
#include "util/theme.hpp"

#include <vlc_input_item.h>

#include <math.h>

#include <QHBoxLayout>
#include <QMenu>
#include <QStyle>
#include <QWidgetAction>

SoundWidget::SoundWidget( QWidget *_parent, intf_thread_t * _p_intf,
                          bool b_shiny, bool b_special )
                         : QWidget( _parent ), p_intf( _p_intf),
                           b_is_muted( false ), b_ignore_valuechanged( false )
{
    /* We need a layout for this widget */
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setSpacing( Theme::instance()->px( RedesignTokens::Deck::Gap ) );
    layout->setContentsMargins( 0, 0, 0, 0 );

    /* The mute button; a controller makes it a deck button (setupButton) */
    volMuteButton = new QToolButton;
    volMuteButton->setProperty( "hotkey", QByteArray( "key-vol-mute" ) );
    connect( volMuteButton, &QToolButton::clicked, this, &SoundWidget::muteClicked );

    /* We might need a subLayout too */
    QVBoxLayout *subLayout;

    /* Normal View, click on icon mutes */
    if( !b_special )
    {
        volumeMenu = NULL; subLayout = NULL;
        volumeControlWidget = NULL;

        /* And add the button */
        layout->addWidget( volMuteButton, 0, Qt::AlignCenter );
    }
    else
    {
        /* Special view, click on button shows the slider */
        b_shiny = false;

        volumeControlWidget = new QFrame( this );
        subLayout = new QVBoxLayout( volumeControlWidget );
        subLayout->setContentsMargins( 4, 4, 4, 4 );
        volumeMenu = new QMenu( this );

        QWidgetAction *widgetAction = new QWidgetAction( volumeControlWidget );
        widgetAction->setDefaultWidget( volumeControlWidget );
        volumeMenu->addAction( widgetAction );

        /* And add the button */
        layout->addWidget( volMuteButton );
    }

    /* Slider creation: shiny or clean */
    if( b_shiny )
    {
        volumeSlider = new SoundSlider( this,
            config_GetFloat( p_intf, "volume-step" ),
            var_InheritString( p_intf, "qt-slider-colours" ),
            var_InheritInteger( p_intf, "qt-max-volume") );
    }
    else
    {
        volumeSlider = new QSlider( NULL );
        volumeSlider->setAttribute( Qt::WA_MacSmallSize);
        volumeSlider->setOrientation( b_special ? Qt::Vertical
                                                : Qt::Horizontal );
        volumeSlider->setMaximum( var_InheritInteger( p_intf, "qt-max-volume" ) );
    }

    volumeSlider->setFocusPolicy( Qt::TabFocus );
    volumeSlider->setAccessibleName( qtr( "Volume" ) );
    if( b_special )
        subLayout->addWidget( volumeSlider );
    else
        layout->addWidget( volumeSlider, 0, Qt::AlignCenter );

    /* Set the volume from the config */
    float volume = playlist_VolumeGet( THEPL );
    libUpdateVolume( (volume >= 0.f) ? volume : 1.f );
    /* Sync mute status */
    if( playlist_MuteGet( THEPL ) > 0 )
        updateMuteStatus( true );

    /* Volume control connection */
    volumeSlider->setTracking( true );
    connect( volumeSlider, &QAbstractSlider::valueChanged, this, &SoundWidget::valueChangedFilter );
    connect( this, &SoundWidget::valueReallyChanged, this, &SoundWidget::userUpdateVolume );
    connect( THEMIM, &MainInputManager::volumeChanged, this, &SoundWidget::libUpdateVolume );
    connect( THEMIM, &MainInputManager::soundMuteChanged, this, &SoundWidget::updateMuteStatus );
}

void SoundWidget::refreshLabels()
{
    /* DESIGN_SPEC 3: volume_off / volume_down / volume_up, as the prototype */
    const int i_sliderVolume = volumeSlider->value();
    const char *icon = b_is_muted || i_sliderVolume == 0 ? RedesignIcons::IconVolumeOff
                     : i_sliderVolume < 50 ? RedesignIcons::IconVolumeDown
                     : RedesignIcons::IconVolumeUp;
    const QString name = b_is_muted ? qfu( vlc_pgettext( "Tooltip|Unmute", "Unmute" ) )
                                    : qfu( vlc_pgettext( "Tooltip|Mute", "Mute" ) );
    volMuteButton->setIcon( deckIcon( icon ) );
    volMuteButton->setToolTip( name );
    volMuteButton->setAccessibleName( name );
}

/* volumeSlider changed value event slot */
void SoundWidget::userUpdateVolume( int i_sliderVolume )
{
    /* Only if volume is set by user action on slider */
    setMuted( false );
    playlist_VolumeSet( THEPL, i_sliderVolume / 100.f );
    refreshLabels();
}

/* libvlc changed value event slot */
void SoundWidget::libUpdateVolume( float volume )
{
    long i_volume = lroundf(volume * 100.f);
    if( i_volume != volumeSlider->value()  )
    {
        b_ignore_valuechanged = true;
        volumeSlider->setValue( i_volume );
        b_ignore_valuechanged = false;
    }
    refreshLabels();
}

void SoundWidget::valueChangedFilter( int i_val )
{
    /* valueChanged is also emitted when the lib setValue() */
    if ( !b_ignore_valuechanged ) emit valueReallyChanged( i_val );
}

/* libvlc mute/unmute event slot */
void SoundWidget::updateMuteStatus( bool mute )
{
    b_is_muted = mute;

    SoundSlider *soundSlider = qobject_cast<SoundSlider *>(volumeSlider);
    if( soundSlider )
        soundSlider->setMuted( mute );
    refreshLabels();
}

/* the small volume shows its slider in a menu, the others mute */
void SoundWidget::muteClicked()
{
    if( volumeSlider->orientation() != Qt::Vertical )
    {
        setMuted( !b_is_muted );
        return;
    }
    volumeMenu->setFixedHeight( volumeMenu->sizeHint().height() );
    volumeMenu->exec( mapToGlobal( QPoint( width(), height() / 2 ) )
                      - QPoint( 0, volumeMenu->height() / 2 ) );
}

void SoundWidget::setMuted( bool mute )
{
    b_is_muted = mute;
    playlist_t *p_playlist = THEPL;
    playlist_MuteSet( p_playlist, mute );
}

/**
 * Play Button
 **/
void PlayButton::setRound( bool round )
{
    b_round = round;
    setProperty( "role", round ? "play" : "deck" );
    updateButtonIcons( b_playing );
}

void PlayButton::updateButtonIcons( bool playing )
{
    b_playing = playing;
    setIcon( deckIcon( playing ? RedesignIcons::IconPause : RedesignIcons::IconPlay, b_round ) );
    setAccessibleName( playing ? qtr( "Pause" ) : qtr( "Play" ) );
    setToolTip( playing ? qtr( "Pause the playback" )
                        : qtr( I_PLAY_TOOLTIP ) );
}

/* on (accent) while a point is set; the icon stays the deck's */
void AtoB_Button::updateButtonIcons( bool timeA, bool timeB )
{
    setChecked( timeA || timeB );
    if( !timeA && !timeB)
    {
        setToolTip( qtr( "Loop from point A to point B continuously\n"
                         "Click to set point A" ) );
    }
    else if( timeA && !timeB )
    {
        setToolTip( qtr( "Click to set point B" ) );
    }
    else if( timeA && timeB )
    {
        setToolTip( qtr( "Stop the A to B loop" ) );
    }
}

void LoopButton::updateButtonIcons( int value )
{
    setChecked( value != NORMAL );
    setIcon( deckIcon( value == REPEAT_ONE ? RedesignIcons::IconRepeatOne
                                           : RedesignIcons::IconRepeat ) );
}

AspectRatioBadge::AspectRatioBadge( intf_thread_t *_p_intf )
    : p_intf( _p_intf ), p_vout( NULL )
{
    setProperty( "role", "speed" );     /* the deck's text buttons */
    setAccessibleName( qtr( "Aspect ratio" ) );
    setToolTip( qtr( "Aspect ratio" ) );
    setPopupMode( QToolButton::InstantPopup );
    menu = new QMenu( this );
    setMenu( menu );
    connect( menu, &QMenu::triggered, this, &AspectRatioBadge::choose );
    connect( THEMIM->getIM(), &InputManager::voutChanged, this, &AspectRatioBadge::setVout );
    setVout();
}

AspectRatioBadge::~AspectRatioBadge()
{
    if( p_vout )
    {
        /* returns once no callback runs: none queues to us after it */
        var_DelCallback( p_vout, "aspect-ratio", AspectCB, this );
        vlc_object_release( p_vout );
    }
}

/* vout thread: only a queued call to the Qt thread (a hotkey changed it) */
int AspectRatioBadge::AspectCB( vlc_object_t *, const char *, vlc_value_t,
                                vlc_value_t, void *data )
{
    QMetaObject::invokeMethod( static_cast<AspectRatioBadge *>( data ), "updateRatios",
                               Qt::QueuedConnection );
    return VLC_SUCCESS;
}

void AspectRatioBadge::setVout()
{
    vout_thread_t *p_new = THEMIM->getVout();   /* held */
    if( p_new != p_vout )
    {
        if( p_vout )
        {
            var_DelCallback( p_vout, "aspect-ratio", AspectCB, this );
            vlc_object_release( p_vout );
        }
        p_vout = p_new;
        if( p_vout )
            var_AddCallback( p_vout, "aspect-ratio", AspectCB, this );
    }
    else if( p_new )
        vlc_object_release( p_new );
    updateRatios();
}

/* The playing video's own ratio, as the nearest of the choices' names
 * ("16:9") or else as "2.00:1"; empty while its size is unknown.
 * shortcut: the size the demuxer reported to the item, so a stream that
 * only the decoder sizes shows "Default"; take the vout's format once 3.0
 * exposes it to the interface. */
static QString pictureRatio( intf_thread_t *p_intf, const vlc_value_t &val_list )
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input )
        return QString();
    const int64_t id = var_GetInteger( p_input, "video-es" );
    input_item_t *item = input_GetItem( p_input );
    double ratio = 0.;
    vlc_mutex_lock( &item->lock );
    for( int i = 0; i < item->i_es; i++ )
    {
        const es_format_t *es = item->es[i];
        if( es->i_cat != VIDEO_ES || ( ratio > 0. && es->i_id != id ) )
            continue;
        const video_format_t &v = es->video;
        const unsigned w = v.i_visible_width ? v.i_visible_width : v.i_width;
        const unsigned h = v.i_visible_height ? v.i_visible_height : v.i_height;
        if( w && h )
            ratio = (double)w * ( v.i_sar_num ? v.i_sar_num : 1 )
                  / ( (double)h * ( v.i_sar_den ? v.i_sar_den : 1 ) );
        if( es->i_id == id )
            break;
    }
    vlc_mutex_unlock( &item->lock );
    if( ratio <= 0. )
        return QString();

    /* the choices are "num:den" ("16:9", "2.35:1") */
    QString best;
    double bestError = .02;  /* within 2 % */
    for( int i = 0; i < val_list.p_list->i_count; i++ )
    {
        const QStringList parts = qfu( val_list.p_list->p_values[i].psz_string ).split( ':' );
        if( parts.count() != 2 || parts[1].toDouble() <= 0. )
            continue;
        const double error = fabs( parts[0].toDouble() / parts[1].toDouble() / ratio - 1. );
        if( error < bestError )
        {
            bestError = error;
            best = parts.join( ':' );
        }
    }
    return best.isEmpty() ? QString( "%1:1" ).arg( ratio, 0, 'f', 2 ) : best;
}

void AspectRatioBadge::updateRatios()
{
    menu->clear();
    /* Disable if there is no vout */
    if( p_vout == NULL )
    {
        setText( qtr( "Default" ) );
        setProperty( "on", false );
        setDisabled( true );
        style()->unpolish( this );
        style()->polish( this );
        return;
    }

    vlc_value_t val_list, text_list;
    char *current = var_GetString( p_vout, "aspect-ratio" );
    /* a value that is not a choice (--aspect-ratio) shows as it is */
    setText( EMPTY_STR( current ) ? qtr( "Default" ) : qfu( current ) );
    var_Change( p_vout, "aspect-ratio", VLC_VAR_GETCHOICES, &val_list, &text_list );
    /* Default: the picture's own ratio, when its size is known */
    if( EMPTY_STR( current ) )
    {
        const QString own = pictureRatio( p_intf, val_list );
        if( !own.isEmpty() )
            setText( own );
    }
    for( int i = 0; i < val_list.p_list->i_count; i++ )
    {
        const char *value = val_list.p_list->p_values[i].psz_string;
        QAction *action = menu->addAction( qfu( text_list.p_list->p_values[i].psz_string ) );
        action->setData( qfu( value ) );
        action->setCheckable( true );
        if( current && !strcmp( value, current ) )
        {
            action->setChecked( true );
            setText( action->text() );
        }
    }
    var_FreeList( &val_list, &text_list );
    setProperty( "on", !EMPTY_STR( current ) );
    free( current );
    setEnabled( true );
    style()->unpolish( this );
    style()->polish( this );
}

void AspectRatioBadge::choose( QAction *action )
{
    if( p_vout )
        var_SetString( p_vout, "aspect-ratio", qtu( action->data().toString() ) );
}

