/*****************************************************************************
 * Copyright © 2011 VideoLAN
 * $Id$
 *
 * Authors: Filipe Azevedo, aka PasNox
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "RoundButton.hpp"
#include "util/theme.hpp"

#include <QPainter>
#include <QStyleOptionToolButton>

RoundButton::RoundButton( QWidget* parent )
            : QToolButton( parent )
{
    setIconSize( QSize( 24, 24 ) );
    setIcon( QIcon::fromTheme( "media-playback-start" ) );
}

QSize RoundButton::sizeHint() const
{
    return QSize( 38, 38 );
}

QBrush RoundButton::pen( QStyleOptionToolButton* option ) const
{
    const bool over = option->state & QStyle::State_MouseOver;
    const Theme *theme = Theme::instance();
    return QBrush( over ? theme->color( Theme::Accent )
                        : theme->ink( RedesignTokens::InkAlpha::Outline ) );
}

QBrush RoundButton::brush( QStyleOptionToolButton* option ) const
{
    const bool over = option->state & QStyle::State_MouseOver;
    const bool pressed = option->state & QStyle::State_Sunken;
    const Theme *theme = Theme::instance();
    return QBrush( theme->color( pressed ? Theme::Hover2 : over ? Theme::Hover : Theme::Raised ) );
}

void RoundButton::paintEvent( QPaintEvent* event )
{
    /*QToolButton::paintEvent( event );
    return;*/

    Q_UNUSED( event );

    QPainter painter( this );
    QStyleOptionToolButton option;

    initStyleOption( &option );
    painter.setRenderHint( QPainter::Antialiasing );

    painter.setPen( QPen( pen( &option ), 1.5 ) );
    painter.setBrush( brush( &option ) );
    painter.drawEllipse( rect().adjusted( 1, 1, -1, -1 ) );

    style()->drawControl( QStyle::CE_ToolButtonLabel, &option, &painter, this );
}

