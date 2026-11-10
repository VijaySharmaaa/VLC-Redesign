/*****************************************************************************
 * drop_overlay.cpp : "Drop to play" frame shown while files are dragged in
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

#include "components/drop_overlay.hpp"
#include "util/theme.hpp"

#include <QPainter>
#include <QPropertyAnimation>

namespace O = RedesignTokens::Overlay;

DropOverlay::DropOverlay( QWidget *window )
    : OverlayWindow( window ), active( false )
{
    setFont( Theme::instance()->font( Theme::PanelTitle ) );
    setAccessibleName( qtr( "Drop to play" ) );
    host = new OverlayHost( window, this );
    connect( host, &OverlayHost::changed, this, &DropOverlay::place );

    fade = new QPropertyAnimation( this, "windowOpacity", this );
    fade->setStartValue( 0. );
    fade->setEndValue( 1. );
    fade->setEasingCurve( enterCurve() );
}

void DropOverlay::setActive( bool on )
{
    if( on == active )
        return;
    active = on;
    fade->stop();
    if( !on )
    {
        hide();
        return;
    }
    const int ms = Theme::instance()->duration( Theme::DropTarget );
    setWindowOpacity( ms ? 0. : 1. );
    place();
    if( ms && isVisible() )
    {
        fade->setDuration( ms );
        fade->start();
    }
}

void DropOverlay::place()
{
    if( !active )
        return;
    if( !host->targetShown() )
    {
        hide();
        return;
    }
    setGeometry( host->targetRect() );
    show();
}

void DropOverlay::paintEvent( QPaintEvent * )
{
    Theme *theme = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );

    /* dashed accent frame on an accent wash */
    QColor wash = theme->color( Theme::Accent );
    wash.setAlpha( O::DropFillAlpha );
    const int frameRadius = theme->metric( Theme::RadiusXl );
    const qreal half = O::DropBorder / 2.;
    p.setPen( QPen( theme->color( Theme::Accent ), O::DropBorder, Qt::DashLine ) );
    p.setBrush( wash );
    p.drawRoundedRect( QRectF( rect() ).adjusted( O::DropInset + half, O::DropInset + half,
                                                  -O::DropInset - half, -O::DropInset - half ),
                       frameRadius, frameRadius );

    /* centred card: icon over the label */
    const QString label = qtr( "Drop to play" );
    const QFontMetrics fm = fontMetrics();
    const QSize card( qMax( O::DropIcon, fm.width( label ) ) + 2 * O::DropCardPadX,
                      O::DropIcon + O::DropGap + fm.height() + 2 * O::DropCardPadY );
    const QRect cardRect( rect().center() - QPoint( card.width() / 2, card.height() / 2 ), card );
    const int cardRadius = theme->metric( Theme::RadiusTile );
    p.setPen( Qt::NoPen );
    p.setBrush( theme->color( Theme::Surface ) );
    p.drawRoundedRect( cardRect, cardRadius, cardRadius );

    const QSize icon( O::DropIcon, O::DropIcon );
    theme->icon( "file_open", Theme::Accent, icon )
        .paint( &p, QRect( QPoint( cardRect.center().x() - O::DropIcon / 2,
                                   cardRect.top() + O::DropCardPadY ), icon ) );
    p.setPen( theme->color( Theme::Text ) );
    p.drawText( QRect( cardRect.left(), cardRect.top() + O::DropCardPadY + O::DropIcon + O::DropGap,
                       cardRect.width(), fm.height() ),
                Qt::AlignCenter, label );
}
