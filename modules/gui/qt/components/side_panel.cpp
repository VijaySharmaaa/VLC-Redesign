/*****************************************************************************
 * side_panel.cpp : a panel docked at the right of the content, a drawer at XS
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

#include "components/side_panel.hpp"
#include "components/overlay_host.hpp"
#include "util/theme.hpp"

#include <QBoxLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QVariantAnimation>

using namespace RedesignTokens;

SidePanel::SidePanel( QBoxLayout *_dock, QWidget *over )
    : QWidget( _dock ? _dock->parentWidget() : NULL ), dock( _dock ),
      dockParent( _dock ? _dock->parentWidget() : NULL ), content( NULL ), slide( NULL ),
      drawer( false ), host( NULL ), fullWidth( 0 ), shownWidth( 0 )
{
    hide();
    content = new QWidget( this );
    if( !dock )
        return;

    /* the drawer's shadow is see-through; paintEvent fills the panel */
    setAttribute( Qt::WA_TranslucentBackground );
    dock->addWidget( this );

    host = new OverlayHost( over, this );
    connect( host, &OverlayHost::changed, this, &SidePanel::place );

    slide = new QVariantAnimation( this );
    slide->setEasingCurve( OverlayWindow::enterCurve() );
    connect( slide, &QVariantAnimation::valueChanged, this,
             [this]( const QVariant &v ) { setShownWidth( v.toInt() ); } );
}

bool SidePanel::setDocked( bool toDrawer, int width )
{
    fullWidth = width;
    const bool changed = toDrawer != drawer;
    if( changed )
    {
        const bool shown = isVisible();
        /* the panel moves, never the video: its native window stays put */
        drawer = toDrawer;
        if( drawer )
        {
            dock->removeWidget( this );
            setParent( dockParent->window(),
                       Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint );
            /* the docked fixed width would clamp the drawer's geometry */
            setMinimumWidth( 0 );
            setMaximumWidth( QWIDGETSIZE_MAX );
        }
        else
        {
            /* Qt::Widget drops the drawer's FramelessWindowHint: with it, a
             * native child of an alpha (translucent) format is flushed as a
             * layered window, which a child cannot be: it stays blank */
            setParent( dockParent, Qt::Widget );
            dock->addWidget( this );
        }
        if( shown )
            show();   /* setParent() hides */
    }
    slide->stop();
    setShownWidth( fullWidth );
    return changed;
}

void SidePanel::slideIn()
{
    slide->stop();
    const int ms = Theme::instance()->duration( Theme::PanelSlide );
    setShownWidth( ms ? 0 : fullWidth );
    if( ms )
    {
        slide->setDuration( ms );
        slide->setStartValue( 0 );
        slide->setEndValue( fullWidth );
        slide->start();
    }
}

void SidePanel::setShownWidth( int w )
{
    shownWidth = w;
    if( drawer )
        place();
    else
        setFixedWidth( w );
}

int SidePanel::shadowWidth() const
{
    return drawer ? Theme::instance()->px( Playlist::DrawerShadow ) : 0;
}

/* The drawer: over the right of the target, the shadow to its left */
void SidePanel::place()
{
    if( !drawer )
        return;
    const QRect target = host->targetRect();
    const int w = shownWidth + shadowWidth();
    /* shortcut: always on the right, as in a left-to-right layout; mirror
     * it with the dock once right-to-left layouts are checked */
    setGeometry( target.right() + 1 - w, target.top(), w, target.height() );
}

void SidePanel::resizeEvent( QResizeEvent * )
{
    if( dock )
        content->setGeometry( shadowWidth(), 0, fullWidth, height() );
}

void SidePanel::paintSheet( QPainter &p, const QRect &sheet )
{
    p.fillRect( sheet, Theme::instance()->color( Theme::Panel ) );
}

void SidePanel::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    const int s = shadowWidth();
    if( s )
    {
        /* shortcut: a linear ramp for the prototype's 50 px blur */
        QColor shadow( Qt::black );
        shadow.setAlpha( Playlist::DrawerShadowAlpha );
        QLinearGradient ramp( 0, 0, s, 0 );
        ramp.setColorAt( 0, Qt::transparent );
        ramp.setColorAt( 1, shadow );
        p.fillRect( 0, 0, s, height(), ramp );
    }
    paintSheet( p, QRect( s, 0, width() - s, height() ) );
    /* the hairline towards the content */
    if( dock )
        p.fillRect( s, 0, Size::Hairline, height(), t->ink( InkAlpha::Divider ) );
}
