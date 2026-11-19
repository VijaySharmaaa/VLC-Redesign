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
#include "util/customwidgets.hpp"   /* setStyleProperty */
#include "util/redesign_icons.hpp"
#include "util/breakpoints.hpp"

#include <QAccessibleWidget>
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

    /* Decision P6-3: the window's own caption buttons (the non-client ones
     * Windows reports) are the ones screen readers get; the drawn copies
     * are left out of the bar's children, the mouse still uses them. */
    class TitleBarAccessible : public QAccessibleWidget
    {
    public:
        explicit TitleBarAccessible( QWidget *w ) : QAccessibleWidget( w ) {}
        int childCount() const Q_DECL_OVERRIDE { return children().count(); }
        QAccessibleInterface *child( int index ) const Q_DECL_OVERRIDE
        {
            QWidget *w = children().value( index );
            return w ? QAccessible::queryAccessibleInterface( w ) : NULL;
        }
        int indexOfChild( const QAccessibleInterface *child ) const Q_DECL_OVERRIDE
        {
            return child ? children().indexOf( qobject_cast<QWidget *>( child->object() ) ) : -1;
        }

    private:
        QList<QWidget *> children() const
        {
            QList<QWidget *> list;
            foreach( QWidget *w, widget()->findChildren<QWidget *>( QString(), Qt::FindDirectChildrenOnly ) )
                if( !w->isWindow() && !w->property( "captionButton" ).toBool() )
                    list << w;
            return list;
        }
    };

    QAccessibleInterface *titleBarFactory( const QString &className, QObject *o )
    {
        if( className == QLatin1String( "TitleBar" ) && o && o->isWidgetType() )
            return new TitleBarAccessible( static_cast<QWidget *>( o ) );
        return NULL;
    }
}

TitleBar::TitleBar( QWidget *_window )
    : QWidget( _window ), window( _window ),
      b_captionButtons( false ), b_minimal( false ), b_fullScreen( false ),
      b_videoFullScreen( false ), b_menus( false )
{
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );

    /* every time: Qt skips a factory it has, and forgets them all when the
     * application object goes (the interface closed and opened again) */
    QAccessible::installFactory( titleBarFactory );

    Theme *t = Theme::instance();
    cone = addButton( qtr( "Go to library" ) );
    connect( cone, &QToolButton::clicked, this, &TitleBar::libraryRequested );

    bar = new QMenuBar( this );
    bar->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Fixed );
    layout->addWidget( bar, 0, Qt::AlignVCenter );
    layout->addStretch();

    minButton = addButton( qtr( "Minimize" ) );
    maxButton = addButton( QString() );
    closeButton = addButton( qtr( "Close" ) );
    closeButton->setObjectName( "captionClose" );
    foreach( QToolButton *b, QList<QToolButton *>() << minButton << maxButton << closeButton )
        b->setProperty( "captionButton", true );
    connect( minButton, &QToolButton::clicked, window, &QWidget::showMinimized );
    connect( maxButton, &QToolButton::clicked, this, &TitleBar::toggleMaximized );
    connect( closeButton, &QToolButton::clicked, window, &QWidget::close );

    window->installEventFilter( this );
    /* another screen's scale changes the sizes too */
    connect( t, &Theme::changed, this, [this]() { updateIcons(); updateLayout(); } );
    updateIcons();
    updateLayout();
}

QToolButton *TitleBar::addButton( const QString &name )
{
    QToolButton *button = new QToolButton( this );
    button->setProperty( "role", "caption" );
    button->setAutoRaise( true );
    /* Like native caption buttons, keyboard users have Alt+Space (and the
     * library its shortcut); a focusable first child would also take the
     * window's initial focus, and with it Space (play/pause). */
    button->setFocusPolicy( Qt::NoFocus );
    button->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Expanding );
    button->setAccessibleName( name );
    button->setToolTip( name );
    layout()->addWidget( button );
    return button;
}

void TitleBar::updateIcons()
{
    Theme *t = Theme::instance();
    const QSize coneSize( t->px( Size::TitleCone ), t->px( Size::TitleCone ) );
    const QSize glyph( t->px( Size::CaptionGlyph ), t->px( Size::CaptionGlyph ) );
    const QSize small( t->px( Size::CaptionGlyphSmall ), t->px( Size::CaptionGlyphSmall ) );

    /* the widths here, with the sizes: they follow the screen's scale */
    cone->setFixedWidth( t->px( Size::TitleConeW ) );
    foreach( QToolButton *b, QList<QToolButton *>() << minButton << maxButton << closeButton )
        b->setFixedWidth( t->metric( Theme::CaptionButtonW ) );
    cone->setIconSize( coneSize );
    /* the original VLC cone, as the app icon (owner's choice over cone.svg) */
    QIcon logo;
    foreach( const char *size, QList<const char *>() << "16" << "48" << "128" << "256" )
        logo.addFile( QString( ":/logo/vlc%1.png" ).arg( size ) );
    cone->setIcon( logo );
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
        ? restoreIcon( t->color( Theme::Text2 ), small.width() )
        : t->icon( RedesignIcons::IconMaximize, Theme::Text2, small ) );
    maxButton->setAccessibleName( name );
    maxButton->setToolTip( name );
    update();
}

void TitleBar::updateLayout()
{
    const bool strip = b_minimal && ( b_captionButtons || b_menus );
    setFixedHeight( Theme::instance()->metric( strip ? Theme::MenuTitleH : Theme::TitleBar ) );
    cone->setVisible( !b_minimal );
    bar->setVisible( !b_minimal || b_menus );
    const bool buttons = b_captionButtons && !b_fullScreen;
    minButton->setVisible( buttons );
    maxButton->setVisible( buttons );
    closeButton->setVisible( buttons );
    setVisible( !b_videoFullScreen && ( !b_minimal || buttons || b_menus ) );
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
    b_menus = false;
    updateLayout();
}

void TitleBar::setFullScreen( bool fullScreen, bool video )
{
    b_fullScreen = fullScreen;
    b_videoFullScreen = fullScreen && video;
    updateLayout();
}

void TitleBar::toggleMenus()
{
    /* also without a strip (a native caption, F11): the bar comes back
     * with the menus only */
    if( !b_minimal || b_videoFullScreen )
        return;
    b_menus = !b_menus;
    updateLayout();
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
    setStyleProperty( maxButton, "nchover", hovered );
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
    p.fillRect( QRect( 0, height() - Size::Hairline, width(), Size::Hairline ),
                t->ink( InkAlpha::Divider ) );

    /* a native caption shows the title itself; minimal view, full screen
     * and the XS breakpoint (as in the design) have no room for it */
    if( !b_captionButtons || b_minimal || b_fullScreen
     || BreakpointWatcher::fromWidth( window->width() ) == BreakpointWatcher::XS )
        return;
    /* between the menus and the window buttons, which a right-to-left
     * layout swaps */
    const int pad = t->px( Size::TitlePadX );
    const int left = ( isRightToLeft() ? minButton->geometry().right()
                                       : bar->geometry().right() ) + 1 + pad;
    const int right = isRightToLeft() ? bar->x() : minButton->x();
    const QRect r( left, 0, right - pad - left, height() );
    if( r.width() <= 0 )
        return;
    p.setFont( t->font( Theme::Meta ) );
    p.setPen( t->color( Theme::Mute ) );
    p.drawText( r, Qt::AlignCenter,
                p.fontMetrics().elidedText( window->windowTitle(), Qt::ElideRight, r.width() ) );
}
