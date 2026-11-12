/*****************************************************************************
 * seekstyle.cpp : Seek slider style
 ****************************************************************************
 * Copyright (C) 2011-2012 VLC authors and VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@videolan.org>
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

#include "seekstyle.hpp"
#include "util/input_slider.hpp"
#include "util/theme.hpp"

#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionSlider>
#include <QPainter>
#include <QDebug>


SeekStyle::SeekStyleOption::SeekStyleOption()
    : QStyleOptionSlider(), buffering( 1.0 ), length(0), animationloading(0.0)
{

}

SeekStyle::SeekStyle() : QProxyStyle( QStyleFactory::create( QLatin1String("Windows") ) )
{

}

int SeekStyle::pixelMetric( PixelMetric metric, const QStyleOption *option, const QWidget *widget ) const
{
    const QStyleOptionSlider *slider;

    if ( widget && ( slider = qstyleoption_cast<const QStyleOptionSlider *>( option ) ) )
    {
        switch( metric )
        {
        case QStyle::PM_SliderThickness:
        case QStyle::PM_SliderLength:
            return widget->minimumSize().height();
        default:
            break;
        }
    }

    return QProxyStyle::pixelMetric( metric, option, widget );
}

void SeekStyle::drawComplexControl( ComplexControl cc, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget ) const
{
    const SeekStyle::SeekStyleOption *slideroptions =
        qstyleoption_cast<const SeekStyle::SeekStyleOption *>( option );
    if( cc != CC_Slider || !slideroptions )
    {
        qWarning() << "SeekStyle: Drawing an unmanaged control";
        QProxyStyle::drawComplexControl( cc, option, painter, widget );
        return;
    }

    /* DESIGN_SPEC 3: a 4 px track (6 hovered), the played part in the
     * accent, chapters as 2 px gaps, a 14 px white knob (16 hovered) in a
     * 3 px accent halo. Sizes and colours from the theme. */
    const Theme *theme = Theme::instance();
    const bool enabled = option->state & QStyle::State_Enabled;
    const bool hover = enabled && ( option->state & QStyle::State_MouseOver );
    const int track = theme->metric( hover ? Theme::SeekTrackHover : Theme::SeekTrack );
    const qreal radius = track / 2.;
    painter->setRenderHints( QPainter::Antialiasing );
    painter->setPen( Qt::NoPen );

    /* the handle's half width at both ends keeps the knob inside */
    QRect groove = subControlRect( CC_Slider, slideroptions, SC_SliderGroove, widget );
    const QRect handle = subControlRect( CC_Slider, slideroptions, SC_SliderHandle, widget );
    groove.adjust( handle.width() / 2, 0, -handle.width() / 2, 0 );
    if( !groove.isValid() || slideroptions->maximum <= 0 )
        return;
    const QRectF bar( groove.left(), option->rect.top() + option->rect.height() / 2. - radius,
                      groove.width(), track );
    const qreal sliderPos = bar.width() * slideroptions->sliderPosition / slideroptions->maximum;

    /* chapter gaps cut through everything the track shows */
    if( slideroptions->points.size() && slideroptions->length > 0 )
    {
        const int gap = theme->px( RedesignTokens::Deck::ChapterGap );
        QRegion clip( bar.toAlignedRect() );
        foreach( int64_t time, slideroptions->points )
        {
            const int x = qRound( bar.left() + time / 1000000.0 / slideroptions->length * bar.width() );
            if( x > bar.left() + gap && x < bar.right() - gap )
                clip -= QRegion( x - gap / 2, bar.top() - 1, gap, track + 2 ); /* +1 px: antialiasing */
        }
        painter->setClipRegion( clip );
    }

    painter->setBrush( theme->ink( RedesignTokens::InkAlpha::Track ) );
    painter->drawRoundedRect( bar, radius, radius );

    /* buffering, then the played part over it */
    if( slideroptions->buffering > 0.0 && slideroptions->buffering < 1.0 )
    {
        painter->setBrush( theme->ink( RedesignTokens::InkAlpha::Outline ) );
        painter->drawRoundedRect( QRectF( bar.left(), bar.top(), bar.width() * slideroptions->buffering,
                                          track ), radius, radius );
    }
    if( sliderPos > 0 )
    {
        painter->setBrush( enabled ? theme->color( Theme::AccentFill )
                                   : theme->ink( RedesignTokens::InkAlpha::ToggleOff ) );
        painter->drawRoundedRect( QRectF( bar.left(), bar.top(), sliderPos, track ), radius, radius );
    }

    /* loading: a segment bouncing along the track */
    if( slideroptions->buffering == 0.0 && slideroptions->animationloading > 0.0 )
    {
        const qreal w = bar.width() / 6;
        painter->setBrush( theme->color( Theme::AccentBorder50 ) );
        painter->drawRoundedRect( QRectF( bar.left() + slideroptions->animationloading * ( bar.width() - w ),
                                          bar.top(), w, track ), radius, radius );
    }
    painter->setClipping( false );

    if( ( slideroptions->subControls & SC_SliderHandle ) && enabled )
    {
        const qreal knob = theme->metric( hover ? Theme::SeekKnobHover : Theme::SeekKnob ) / 2.;
        const qreal halo = knob + theme->px( RedesignTokens::Deck::KnobHalo );
        const QPointF centre( bar.left() + sliderPos, bar.center().y() );
        painter->setBrush( theme->color( Theme::AccentKnobHalo ) );
        painter->drawEllipse( centre, halo, halo );
        painter->setBrush( theme->color( Theme::SeekKnobFill ) );
        painter->drawEllipse( centre, knob, knob );
    }
}
