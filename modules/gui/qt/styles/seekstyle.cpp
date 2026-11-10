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
#include "adapters/seekpoints.hpp"
#include "util/theme.hpp"

#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionSlider>
#include <QPainter>
#include <QDebug>

#define RADIUS 3
#define CHAPTERSSPOTSIZE 3

SeekStyle::SeekStyleOption::SeekStyleOption()
    : QStyleOptionSlider(), buffering( 1.0 ), length(0), animate(false), animationopacity( 1.0 ),
      animationloading(0.0)
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
    if( cc == CC_Slider )
    {
        painter->setRenderHints( QPainter::Antialiasing );

        if ( const SeekStyle::SeekStyleOption *slideroptions =
             qstyleoption_cast<const SeekStyle::SeekStyleOption *>( option ) )
        {
            /* colours from the theme (DESIGN_SPEC 3); sizes are still the classic ones */
            const Theme *theme = Theme::instance();
            const bool enabled = option->state & QStyle::State_Enabled;
            qreal sliderPos = -1;

            /* Get the needed subcontrols to draw the slider */
            QRect groove = subControlRect(CC_Slider, slideroptions, SC_SliderGroove, widget);
            QRect handle = subControlRect(CC_Slider, slideroptions, SC_SliderHandle, widget);

            /* Adjust the size of the groove so the handle stays centered */
            groove.adjust( handle.width() / 2, 0, -handle.width() / 2, 0 );

            /* Reduce the height of the groove */
            // Note: in the original 2.0.0 code, the groove had a height of 9px and to
            // comply with the original style (but still allow the widget to expand) I
            // had to remove 1 from the rect bottom.
            groove.adjust( 0, (qreal)groove.height() / 3.7, 0, (qreal)-groove.height() / 3.7 - 1 );

            if ( ( slideroptions->subControls & SC_SliderGroove ) && groove.isValid() )
            {
                sliderPos = ( ( (qreal)groove.width() ) / (qreal)slideroptions->maximum )
                        * (qreal)slideroptions->sliderPosition;

                /* draw the track */
                painter->setPen( Qt::NoPen );
                painter->setBrush( theme->ink( RedesignTokens::InkAlpha::Track ) );
                painter->drawRoundedRect( groove, RADIUS, RADIUS );

                /* adjusted foreground rectangle */
                QRect valueRect = groove.adjusted( 1, 1, -1, 0 );

                valueRect.setWidth( sliderPos );

                /* draw the played part */
                if ( slideroptions->sliderPosition > slideroptions->minimum && slideroptions->sliderPosition <= slideroptions->maximum )
                {
                    painter->setBrush( enabled ? theme->color( Theme::Accent )
                                               : theme->ink( RedesignTokens::InkAlpha::ToggleOff ) );
                    painter->drawRoundedRect( valueRect, RADIUS, RADIUS );
                }

                /* loading: a segment bouncing along the track */
                if ( slideroptions->buffering == 0.0 && slideroptions->animationloading > 0.0 )
                {
                    int width = groove.width() - groove.width() / 6;
                    QRect innerRect = groove.adjusted( slideroptions->animationloading * width + 1, 1,
                            width * ( -1.0 + slideroptions->animationloading ) - 1, 0);
                    painter->setBrush( theme->color( Theme::AccentBorder50 ) );
                    painter->drawRoundedRect( innerRect, RADIUS, RADIUS );
                }

                /* draw buffering overlay */
                if ( slideroptions->buffering > 0.0 && slideroptions->buffering < 1.0 )
                {
                    QRect innerRect = groove.adjusted( 1, 1,
                                        groove.width() * ( -1.0 + slideroptions->buffering ) - 1, 0 );
                    painter->setBrush( theme->ink( RedesignTokens::InkAlpha::Outline ) );
                    painter->drawRoundedRect( innerRect, RADIUS, RADIUS );
                }
            }

            if ( slideroptions->subControls & SC_SliderTickmarks ) {
                QStyleOptionSlider tmpSlider = *slideroptions;
                tmpSlider.subControls = SC_SliderTickmarks;
                QProxyStyle::drawComplexControl(cc, &tmpSlider, painter, widget);
            }

            if ( slideroptions->subControls & SC_SliderHandle && handle.isValid() )
            {
                /* Useful for debugging */
                //painter->setBrush( QColor( 0, 0, 255, 150 ) );
                //painter->drawRect( handle );

                if ( option->state & QStyle::State_MouseOver || slideroptions->animate )
                {
                    /* draw chapters tickpoints */
                    if ( slideroptions->points.size() && slideroptions->length && groove.width() )
                    {
                        if ( slideroptions->orientation == Qt::Horizontal ) /* TODO: vertical */
                        {
                            painter->setPen( theme->color( Theme::Mute ) );
                            painter->setBrush( Qt::NoBrush );
                            foreach( int64_t time, slideroptions->points )
                            {
                                int x = groove.x() + time / 1000000.0 / slideroptions->length * groove.width();
                                painter->drawLine( x, slideroptions->rect.height(), x, slideroptions->rect.height() - CHAPTERSSPOTSIZE );
                            }
                        }
                    }

                    /* draw handle: white knob in an accent halo */
                    if ( enabled && sliderPos != -1 )
                    {
                        QSize hSize = QSize( handle.height(), handle.height() ) - QSize( 6, 6 );
                        QPoint pos = QPoint( handle.center().x() - ( hSize.width() / 2 ), handle.center().y() - ( hSize.height() / 2 ) );

                        QPoint haloPos( pos - QPoint( 2, 2 ) );
                        QSize haloSize( QSize( handle.height(), handle.height() ) - QSize( 2, 2 ) );

                        painter->setPen( Qt::NoPen );
                        painter->setOpacity( slideroptions->animationopacity );

                        painter->setBrush( theme->color( Theme::AccentKnobHalo ) );
                        painter->drawEllipse( haloPos.x(), haloPos.y(), haloSize.width(), haloSize.height() );

                        painter->setBrush( theme->color( Theme::SeekKnobFill ) );
                        painter->drawEllipse( pos.x(), pos.y(), hSize.width(), hSize.height() );
                    }
                }
            }
        }
    }
    else
    {
        qWarning() << "SeekStyle: Drawing an unmanaged control";
        QProxyStyle::drawComplexControl( cc, option, painter, widget );
    }
}
