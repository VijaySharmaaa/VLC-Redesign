/*****************************************************************************
 * toast.cpp : short notice at the bottom of the main window
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

#include "components/toast.hpp"
#include "components/video_overlays.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_variables.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_vout.h>

#include <QPainter>
#include <QStyle>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QAccessible>
#include <QFileInfo>
#include <QDir>

namespace O = RedesignTokens::Overlay;

/* In the thread that took the snapshot (never a vout's own): the core has
 * just put the file name (OSD channel) and the preview (channel 0, only it
 * uses that one) on the video; the toast says it instead (P6-9), when it
 * can show.
 * shortcut: the vouts drop them on their next control pass, so they may
 * show for a frame; upgrade with a core option to skip the snapshot OSD */
int Toast::SnapshotCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t newval, void *data )
{
    Toast *toast = static_cast<Toast *>( data );
    /* hidden content (the mini player, minimised): the OSD is all there is */
    input_thread_t *input = toast->canShow.load()
                          ? playlist_CurrentInput( pl_Get( toast->p_intf ) ) : NULL;
    if( input )
    {
        vout_thread_t **pp_vout;
        size_t i_vout;
        if( !input_Control( input, INPUT_GET_VOUTS, &pp_vout, &i_vout ) )
        {
            for( size_t i = 0; i < i_vout; i++ )
            {
                vout_FlushSubpictureChannel( pp_vout[i], VOUT_SPU_CHANNEL_OSD );
                vout_FlushSubpictureChannel( pp_vout[i], 0 );
                vlc_object_release( pp_vout[i] );
            }
            free( pp_vout );
        }
        vlc_object_release( input );
    }
    emit toast->snapshotSaved( qfu( newval.psz_string ) );
    return VLC_SUCCESS;
}

Toast::Toast( intf_thread_t *_p_intf, QWidget *owner, QWidget *_content )
    : OverlayWindow( owner ), p_intf( _p_intf ), content( _content ), overVideo( false )
{
    setFont( Theme::instance()->font( Theme::Body ) );
    host = new OverlayHost( _content, this );
    canShow = host->targetShown();
    connect( host, &OverlayHost::changed, this, [this]() { canShow = host->targetShown(); } );
    connect( host, &OverlayHost::changed, this, &Toast::place );
    _content->installEventFilter( this );   /* its subtitleLift */

    fade = new QPropertyAnimation( this, "windowOpacity", this );
    fade->setStartValue( 0. );
    fade->setEndValue( 1. );
    rise = new QPropertyAnimation( this, "pos", this );
    rise->setEasingCurve( enterCurve() );
    enter = new QParallelAnimationGroup( this );
    enter->addAnimation( fade );
    enter->addAnimation( rise );

    /* the hold is not motion: it stays when Windows animations are off */
    hold.setSingleShot( true );
    hold.setInterval( RedesignTokens::Motion::ToastHold );
    connect( &hold, &QTimer::timeout, this, &QWidget::hide );

    connect( this, &Toast::snapshotSaved, this, &Toast::onSnapshot, Qt::QueuedConnection );
    var_AddCallback( p_intf->obj.libvlc, "snapshot-file", SnapshotCB, this );
}

Toast::~Toast()
{
    var_DelCallback( p_intf->obj.libvlc, "snapshot-file", SnapshotCB, this );
}

void Toast::onSnapshot( const QString &path )
{
    QFileInfo info( path );
    showMessage( qtr( "Snapshot saved to %1" ).arg(
                 QDir::toNativeSeparators( info.dir().dirName() + "/" + info.fileName() ) ) );
}

void Toast::showMessage( const QString &message )
{
    const bool wasShown = isVisible();
    text = message;
    setAccessibleName( text );
    hold.start();
    if( !host->targetShown() )
        return;         /* place() shows it if the window comes back in time */

    fitText();
    const int ms = Theme::instance()->duration( Theme::Toast );
    if( wasShown || ms == 0 )
        place();
    else
    {
        const QPoint to = restingPos();
        rise->setStartValue( to + QPoint( 0, Theme::instance()->px( RedesignTokens::Motion::ToastOffset ) ) );
        rise->setEndValue( to );
        fade->setDuration( ms );
        rise->setDuration( ms );
        setWindowOpacity( 0. );
        move( rise->startValue().toPoint() );
        show();
        enter->start();
    }
    update();
#ifndef QT_NO_ACCESSIBILITY
    QAccessibleEvent alert( this, QAccessible::Alert );
    QAccessible::updateAccessibility( &alert );
#endif
}

/* the immersive bar lifts the subtitles (or drops them) while a toast shows */
bool Toast::eventFilter( QObject *o, QEvent *e )
{
    if( o == content && e->type() == QEvent::DynamicPropertyChange
     && static_cast<QDynamicPropertyChangeEvent *>( e )->propertyName() == "subtitleLift" )
    {
        if( enter->state() == QAbstractAnimation::Running )
            rise->setEndValue( restingPos() );   /* rises on, to the new place */
        else
            place();
    }
    return OverlayWindow::eventFilter( o, e );
}

void Toast::setOverVideo( bool b )
{
    overVideo = b;
    place();
}

void Toast::place()
{
    if( !hold.isActive() )
        return;
    enter->stop();
    if( !host->targetShown() )
    {
        hide();
        return;
    }
    fitText();
    setWindowOpacity( 1. );
    move( restingPos() );
    show();
}

void Toast::fitText()
{
    const Theme *t = Theme::instance();
    /* the same sum as paintEvent's text rect: px() of the whole would round
     * differently and elide a text that fits */
    const int chrome = 2 * t->px( O::ToastPadX ) + t->px( O::ToastIcon ) + t->px( O::ToastGap );
    const int room = host->targetRect().width() - 2 * t->px( O::ToastMargin );
    resize( qMax( chrome, qMin( room, chrome + fontMetrics().width( text ) ) ),
            qMax( t->px( O::ToastIcon ), fontMetrics().height() ) + 2 * t->px( O::ToastPadY ) );
}

QPoint Toast::restingPos() const
{
    const QRect r = host->targetRect();
    int bottom = r.bottom() + 1 - Theme::instance()->px( O::ToastMargin );
    /* subtitles sit at the bottom of the picture, not of the area (bars
     * around it), and above the immersive bar while it shows */
    if( overVideo && content )
    {
        const QRect pic = videoPictureRect( p_intf, r );
        bottom = qMin( bottom, pic.bottom() + 1 - content->property( "subtitleLift" ).toInt()
                               - pic.height() * O::ToastOverVideoPercent / 100 );
    }
    return QPoint( r.center().x() - width() / 2, bottom - height() );
}

void Toast::paintEvent( QPaintEvent * )
{
    Theme *theme = Theme::instance();
    const int radius = theme->metric( Theme::RadiusLg );
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    p.setPen( theme->ink( O::ToastBorderAlpha ) );
    p.setBrush( theme->color( Theme::Hover ) );
    p.drawRoundedRect( QRectF( rect() ).adjusted( .5, .5, -.5, -.5 ), radius, radius );

    const int iconPx = theme->px( O::ToastIcon ), padX = theme->px( O::ToastPadX );
    const QSize icon( iconPx, iconPx );
    /* the icon leads: on the right in a right-to-left language */
    const Qt::LayoutDirection dir = layoutDirection();
    theme->icon( RedesignIcons::IconInfo, Theme::Accent, icon )
        .paint( &p, QStyle::visualRect( dir, rect(),
                                        QRect( QPoint( padX, ( height() - iconPx ) / 2 ), icon ) ) );

    const QRect textRect = rect().adjusted( padX + iconPx + theme->px( O::ToastGap ), 0,
                                            -padX, 0 );
    p.setPen( theme->color( Theme::Text ) );
    p.drawText( QStyle::visualRect( dir, rect(), textRect ).translated( 0, -theme->textNudge( p.font() ) ),
                QStyle::visualAlignment( dir, Qt::AlignVCenter | Qt::AlignLeft ),
                fontMetrics().elidedText( text, Qt::ElideMiddle, textRect.width() ) );
}
