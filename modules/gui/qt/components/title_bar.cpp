/*****************************************************************************
 * title_bar.cpp : the main window's title bar with its menu bar
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/title_bar.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QPainter>
#include <QStyle>
#include <QToolButton>

using namespace RedesignTokens;

namespace
{
    /* Restore glyph, not in the icon set: two squares on crop_square's
     * grid (Material 24 px, 1 unit = size / 12, 1 unit stroke), the back
     * one 1.5 units up and right, drawn where the front one leaves it. */
    QIcon restoreIcon( const QColor &color, int size )
    {
        const qreal dpr = qApp->devicePixelRatio();
        QPixmap pixmap( QSize( size, size ) * dpr );
        pixmap.setDevicePixelRatio( dpr );
        pixmap.fill( Qt::transparent );
        QPainter p( &pixmap );
        p.setRenderHint( QPainter::Antialiasing );
        const qreal u = size / 12.0, h = u / 2;
        p.setPen( QPen( color, u, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin ) );
        p.drawRect( QRectF( 1.5 * u + h, 3 * u + h, 6.5 * u, 6.5 * u ) );
        const QPointF back[] = {
            QPointF( 3 * u + h, 3 * u ), QPointF( 3 * u + h, 1.5 * u + h ),
            QPointF( 10.5 * u - h, 1.5 * u + h ), QPointF( 10.5 * u - h, 9 * u - h ),
            QPointF( 9 * u, 9 * u - h ),
        };
        p.drawPolyline( back, sizeof( back ) / sizeof( back[0] ) );
        return QIcon( pixmap );
    }
}

TitleBar::TitleBar( QWidget *_window )
    : QWidget( _window ), window( _window ),
      b_captionButtons( false ), b_minimal( false ), b_fullScreen( false )
{
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );

    cone = addButton( qtr( "Go to library" ), Size::TitleConeW );
    connect( cone, &QToolButton::clicked, this, &TitleBar::libraryRequested );

    bar = new QMenuBar( this );
    bar->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    layout->addWidget( bar, 0, Qt::AlignVCenter );
    layout->addStretch();

    minButton = addButton( qtr( "Minimize" ), Size::CaptionButtonW );
    maxButton = addButton( QString(), Size::CaptionButtonW );
    closeButton = addButton( qtr( "Close" ), Size::CaptionButtonW );
    closeButton->setObjectName( "captionClose" );
    connect( minButton, &QToolButton::clicked, window, &QWidget::showMinimized );
    connect( maxButton, &QToolButton::clicked, this, &TitleBar::toggleMaximized );
    connect( closeButton, &QToolButton::clicked, window, &QWidget::close );

    window->installEventFilter( this );
    connect( Theme::instance(), &Theme::changed, this, &TitleBar::updateIcons );
    updateIcons();
    updateLayout();
}

QToolButton *TitleBar::addButton( const QString &name, int width )
{
    QToolButton *button = new QToolButton( this );
    button->setProperty( "role", "caption" );
    button->setAutoRaise( true );
    /* Like native caption buttons, keyboard users have Alt+Space (and the
     * library its shortcut); a focusable first child would also take the
     * window's initial focus, and with it Space (play/pause). */
    button->setFocusPolicy( Qt::NoFocus );
    button->setFixedWidth( width );
    button->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
    button->setAccessibleName( name );
    button->setToolTip( name );
    layout()->addWidget( button );
    return button;
}

void TitleBar::updateIcons()
{
    Theme *t = Theme::instance();
    const QSize coneSize( Size::TitleCone, Size::TitleCone );
    const QSize glyph( Size::CaptionGlyph, Size::CaptionGlyph );
    const QSize small( Size::CaptionGlyphSmall, Size::CaptionGlyphSmall );

    cone->setIconSize( coneSize );
    cone->setIcon( t->icon( RedesignIcons::IconCone, Theme::Text, coneSize ) );
    minButton->setIconSize( glyph );
    minButton->setIcon( t->icon( RedesignIcons::IconMinimize, Theme::Text2, glyph ) );
    QIcon close = t->icon( RedesignIcons::IconClose, Theme::Text2, glyph );
    close.addPixmap( t->icon( RedesignIcons::IconClose, Theme::WindowCloseGlyph, glyph )
                     .pixmap( glyph ), QIcon::Active );
    closeButton->setIconSize( glyph );
    closeButton->setIcon( close );

    const bool maximized = window->isMaximized();
    const QString name = maximized ? qtr( "Restore" ) : qtr( "Maximize" );
    maxButton->setIconSize( small );
    maxButton->setIcon( maximized
        ? restoreIcon( t->color( Theme::Text2 ), Size::CaptionGlyphSmall )
        : t->icon( RedesignIcons::IconMaximize, Theme::Text2, small ) );
    maxButton->setAccessibleName( name );
    maxButton->setToolTip( name );
    update();
}

void TitleBar::updateLayout()
{
    const bool strip = b_minimal && b_captionButtons;
    setFixedHeight( Theme::instance()->metric( strip ? Theme::MenuTitleH : Theme::TitleBar ) );
    cone->setVisible( !b_minimal );
    bar->setVisible( !b_minimal );
    minButton->setVisible( b_captionButtons );
    maxButton->setVisible( b_captionButtons );
    closeButton->setVisible( b_captionButtons );
    setVisible( !b_fullScreen && ( !b_minimal || b_captionButtons ) );
    update();
}

void TitleBar::setCaptionButtonsVisible( bool visible )
{
    b_captionButtons = visible;
    updateLayout();
}

void TitleBar::setMinimal( bool minimal )
{
    b_minimal = minimal;
    updateLayout();
}

void TitleBar::setFullScreen( bool fullScreen )
{
    b_fullScreen = fullScreen;
    updateLayout();
}

void TitleBar::toggleMenus()
{
    if( b_minimal && !isHidden() )
        bar->setVisible( bar->isHidden() );
}

TitleBar::Area TitleBar::areaAt( const QPoint &pos ) const
{
    QWidget *w = childAt( pos );
    if( w == maxButton )
        return MaximizeButton;
    if( !w || ( w == bar && !bar->actionAt( bar->mapFrom( this, pos ) ) ) )
        return Caption;
    return Client;
}

void TitleBar::toggleMaximized()
{
    if( window->isMaximized() )
        window->showNormal();
    else
        window->showMaximized();
}

void TitleBar::setMaximizeHovered( bool hovered )
{
    if( maxButton->property( "nchover" ).toBool() == hovered )
        return;
    maxButton->setProperty( "nchover", hovered );
    /* dynamic-property selectors are only re-read on polish */
    maxButton->style()->unpolish( maxButton );
    maxButton->style()->polish( maxButton );
}

bool TitleBar::eventFilter( QObject *o, QEvent *e )
{
    if( o == window && e->type() == QEvent::WindowTitleChange )
        update();
    else if( o == window && e->type() == QEvent::WindowStateChange )
        updateIcons();
    return QWidget::eventFilter( o, e );
}

void TitleBar::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    p.fillRect( rect(), t->color( Theme::Chrome ) );
    p.fillRect( QRect( 0, height() - 1, width(), 1 ), t->ink( InkAlpha::Divider ) );

    /* a native caption shows the title itself; minimal view has no room */
    if( !b_captionButtons || b_minimal )
        return;
    const int left = bar->geometry().right() + 1 + Size::TitlePadX;
    const QRect r( left, 0, minButton->x() - Size::TitlePadX - left, height() );
    if( r.width() <= 0 )
        return;
    p.setFont( t->font( Theme::Meta ) );
    p.setPen( t->color( Theme::Mute ) );
    p.drawText( r, Qt::AlignCenter,
                p.fontMetrics().elidedText( window->windowTitle(), Qt::ElideRight, r.width() ) );
}
