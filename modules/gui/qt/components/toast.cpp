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
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_variables.h>

#include <QPainter>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QAccessible>
#include <QFileInfo>
#include <QDir>

namespace O = RedesignTokens::Overlay;

static int SnapshotCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t newval, void *data )
{
    emit static_cast<Toast *>( data )->snapshotSaved( qfu( newval.psz_string ) );
    return VLC_SUCCESS;
}

Toast::Toast( intf_thread_t *_p_intf, QWidget *owner, QWidget *content )
    : OverlayWindow( owner ), p_intf( _p_intf ), overVideo( false )
{
    setFont( Theme::instance()->font( Theme::Body ) );
    host = new OverlayHost( content, this );
    connect( host, &OverlayHost::changed, this, &Toast::place );

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
    const int chrome = t->px( 2 * O::ToastPadX + O::ToastIcon + O::ToastGap );
    const int room = host->targetRect().width() - 2 * t->px( O::ToastMargin );
    resize( qMax( chrome, qMin( room, chrome + fontMetrics().width( text ) ) ),
            qMax( t->px( O::ToastIcon ), fontMetrics().height() ) + 2 * t->px( O::ToastPadY ) );
}

QPoint Toast::restingPos() const
{
    const QRect r = host->targetRect();
    int margin = Theme::instance()->px( O::ToastMargin );
    if( overVideo )
        margin = qMax( margin, r.height() * O::ToastOverVideoPercent / 100 );
    return QPoint( r.center().x() - width() / 2, r.bottom() + 1 - margin - height() );
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
    theme->icon( RedesignIcons::IconInfo, Theme::Accent, icon )
        .paint( &p, QRect( QPoint( padX, ( height() - iconPx ) / 2 ), icon ) );

    const QRect textRect = rect().adjusted( padX + iconPx + theme->px( O::ToastGap ), 0,
                                            -padX, 0 );
    p.setPen( theme->color( Theme::Text ) );
    p.drawText( textRect, Qt::AlignVCenter | Qt::AlignLeft,
                fontMetrics().elidedText( text, Qt::ElideMiddle, textRect.width() ) );
}
