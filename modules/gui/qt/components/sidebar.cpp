/*****************************************************************************
 * sidebar.cpp : navigation sidebar of the main interface
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

#include "qt.hpp"
#include "components/sidebar.hpp"
#include "components/library/home_page.hpp"
#include "components/library/media_page.hpp"
#include "components/library/network_page.hpp"
#include "components/prefs_page.hpp"
#include "dialogs_provider.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QVBoxLayout>

using namespace RedesignTokens;

static_assert( Size::SidebarRailBelow == Breakpoint::M,
               "the sidebar turns into a rail at the M breakpoint" );

/* One sidebar entry, painted from tokens: the stylesheet cannot left-align
 * a tool button's content nor set the 12 px icon-label gap. */
class SidebarButton : public QAbstractButton
{
public:
    SidebarButton( const QString &label, const char *_icon, bool _primary,
                   QWidget *parent )
        : QAbstractButton( parent ), icon( _icon ), primary( _primary ), rail( false )
    {
        setText( label );
        setAccessibleName( label );
        /* Tab only: a click must not take the keys from the player */
        setFocusPolicy( Qt::TabFocus );
        setAttribute( Qt::WA_Hover );
    }

    void setRail( bool b ) { rail = b; update(); }

    QSize sizeHint() const Q_DECL_OVERRIDE
    {
        const int h = Theme::instance()->metric( primary ? Theme::SidebarOpenBtnH
                                                         : Theme::SidebarItemH );
        return QSize( h, h );
    }

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    const char *icon;
    bool primary;
    bool rail;
};

void SidebarButton::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    const int radius = primary ? t->px( Size::SidebarOpenRadius ) : t->metric( Theme::RadiusMd );

    QColor fill( Qt::transparent );
    if( primary )
        fill = t->color( underMouse() ? Theme::AccentHover : Theme::Accent );
    else if( isDown() )
        fill = t->ink( InkAlpha::OpenFill );
    else if( isChecked() )
        fill = t->color( Theme::AccentTint12 );
    else if( underMouse() )
        fill = t->ink( InkAlpha::HoverFill );
    p.setPen( Qt::NoPen );
    p.setBrush( fill );
    p.drawRoundedRect( rect(), radius, radius );

    if( hasFocus() )
    {
        /* the accent is 2.4:1 on light surfaces and invisible on the accent */
        const Theme::ColorRole ring = primary ? Theme::OnAccent
                                    : t->isDark() ? Theme::Accent : Theme::AccentText;
        const qreal inset = Size::FocusRing / 2.;
        p.setPen( QPen( t->color( ring ), Size::FocusRing ) );
        p.setBrush( Qt::NoBrush );
        p.drawRoundedRect( QRectF( rect() ).adjusted( inset, inset, -inset, -inset ),
                           radius - inset, radius - inset );
    }

    const Theme::ColorRole ink = primary ? Theme::OnAccent
                               : isChecked() ? Theme::AccentText : Theme::Text2;
    QFont font = t->font( Theme::Body );
    if( primary )
        font.setWeight( QFont::DemiBold );
    const QFontMetrics fm( font );

    /* rail: the icon alone, centred; primary: icon and label centred as a
     * group; nav item: icon and label from the left */
    const int iconPx = t->px( Size::SidebarIcon );
    const int gap = t->px( primary ? Size::SidebarOpenIconGap : Size::SidebarIconGap );
    const int padX = t->px( Size::SidebarItemPadX );
    int x;
    if( rail )
        x = ( width() - iconPx ) / 2;
    else if( primary )
        x = ( width() - iconPx - gap - fm.width( text() ) ) / 2;
    else
        x = padX;
    const QRect iconRect( x, ( height() - iconPx ) / 2, iconPx, iconPx );
    /* QIcon::paint picks the device-pixel size: render the tint at that size */
    const int devicePx = qRound( iconPx * devicePixelRatioF() );
    t->icon( QLatin1String( icon ), ink, QSize( devicePx, devicePx ) )
        .paint( &p, QStyle::visualRect( layoutDirection(), rect(), iconRect ) );
    if( rail )
        return;

    const QRect textRect( iconRect.right() + 1 + gap, 0,
                          width() - iconRect.right() - 1 - gap - padX, height() );
    p.setFont( font );
    p.setPen( t->color( ink ) );
    p.drawText( QStyle::visualRect( layoutDirection(), rect(), textRect ),
                Qt::AlignVCenter | Qt::AlignLeft,
                fm.elidedText( text(), Qt::ElideRight, textRect.width() ) );
}

/* Phase 4 replaces this with the Home page */
static QWidget *createPage( const QString &title )
{
    QWidget *page = new QWidget;
    page->setAccessibleName( title );
    QVBoxLayout *layout = new QVBoxLayout( page );
    const Theme *t = Theme::instance();
    layout->setContentsMargins( t->px( Size::PagePadX ), t->px( Size::PagePadTop ),
                                t->px( Size::PagePadX ), t->px( Size::PagePadX ) );
    QLabel *label = new QLabel( title, page );
    label->setFont( t->font( Theme::PageTitle ) );
    layout->addWidget( label );
    layout->addStretch( 1 );
    return page;
}

/* "Label (shortcut)", the shortcut as menus.cpp binds it */
static QString tip( const QString &label, const char *shortcut )
{
    return QString( "%1 (%2)" ).arg( label,
        QKeySequence( QLatin1String( shortcut ) ).toString( QKeySequence::NativeText ) );
}

Sidebar::Sidebar( intf_thread_t *p_intf, QWidget *parent )
    : QWidget( parent ), currentPage( NULL )
{
    setAccessibleName( qtr( "Library" ) );
    QVBoxLayout *layout = new QVBoxLayout( this );
    const Theme *t = Theme::instance();
    layout->setContentsMargins( t->px( Size::SidebarPadX ), t->px( Size::SidebarPadY ),
                                t->px( Size::SidebarPadX ), t->px( Size::SidebarPadY ) );
    layout->setSpacing( t->px( Size::SidebarItemGap ) );

    /* shortcut: the keys are copied from menus.cpp (Open Multiple Files,
     * Preferences), read them from the menu actions once lane A exposes them */
    SidebarButton *open = new SidebarButton( qtr( "Open media" ),
                                             RedesignIcons::IconOpenMedia, true, this );
    open->setToolTip( tip( qtr( "Open media" ), "Ctrl+Shift+O" ) );
    connect( open, &QAbstractButton::clicked, this, []() { THEDP->openDialog(); } );
    layout->addWidget( open );
    layout->addSpacing( t->px( Size::SidebarOpenBelow ) );
    buttons.append( open );

    static const struct { const char *label; const char *icon; } nav[Preferences] = {
        { N_( "Home" ),    RedesignIcons::IconNavHome },
        { N_( "Videos" ),  RedesignIcons::IconNavVideos },
        { N_( "Music" ),   RedesignIcons::IconNavMusic },
        { N_( "Network" ), RedesignIcons::IconNavNetwork },
    };
    group = new QButtonGroup( this ); /* exclusive; arrow keys move within it */
    for( int i = 0; i < Preferences; i++ )
    {
        const QString label = qtr( nav[i].label );
        switch( i )
        {
        case Home:    pages[i] = new HomePage( p_intf ); break;
        case Videos:  pages[i] = new MediaPage( p_intf, MediaPage::Videos ); break;
        case Music:   pages[i] = new MediaPage( p_intf, MediaPage::Music ); break;
        case Network: pages[i] = new NetworkPage( p_intf ); break;
        default:      pages[i] = createPage( label ); break;
        }
        SidebarButton *b = new SidebarButton( label, nav[i].icon, false, this );
        b->setCheckable( true );
        b->setAutoExclusive( true ); /* screen readers: a radio button; the group rules */
        b->setToolTip( label );
        group->addButton( b, i );
        connect( b, &QAbstractButton::clicked, this,
                 [this, i]() { emit pageRequested( pages[i] ); } );
        layout->addWidget( b );
        buttons.append( b );
    }

    layout->addStretch( 1 );
    SidebarButton *prefs = new SidebarButton( qtr( "Preferences" ),
                                              RedesignIcons::IconSettings, false, this );
    prefs->setToolTip( tip( qtr( "Preferences" ), "Ctrl+P" ) );
    prefs->setCheckable( true );
    prefs->setAutoExclusive( true );
    group->addButton( prefs, Preferences );
    pages[Preferences] = new PrefsPage( p_intf );
    /* the menu's way, which opens the complete tree in a dialog when it is
     * asked for: then the item the click checked goes back */
    connect( prefs, &QAbstractButton::clicked, this, [this]() {
        THEDP->prefsDialog();
        setCurrentWidget( currentPage );
    } );
    layout->addWidget( prefs );
    buttons.append( prefs );

    connect( Theme::instance(), &Theme::changed, this, [this]() { update(); } );
}

void Sidebar::showPage( int i )
{
    if( i >= 0 && i < PageCount )
        emit pageRequested( pages[i] );
}

void Sidebar::setCurrentWidget( QWidget *w )
{
    currentPage = w;
    for( int i = 0; i < PageCount; i++ )
        if( pages[i] == w )
        {
            group->button( i )->setChecked( true );
            return;
        }
    QAbstractButton *checked = group->checkedButton();
    if( checked )
    {
        /* an exclusive group cannot be emptied */
        group->setExclusive( false );
        checked->setChecked( false );
        group->setExclusive( true );
    }
}

int Sidebar::widthAt( BreakpointWatcher::Size bp )
{
    static const int widths[] = { Size::SidebarXS, Size::SidebarS, Size::SidebarM, Size::SidebarL };
    return Theme::instance()->px( widths[bp] );
}

void Sidebar::setBreakpoint( BreakpointWatcher::Size bp )
{
    setFixedWidth( widthAt( bp ) );
    for( SidebarButton *b : buttons )
        b->setRail( bp < BreakpointWatcher::M );
}

void Sidebar::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    p.fillRect( rect(), t->color( Theme::Chrome ) );
    /* the hairline towards the content */
    p.fillRect( QRect( isRightToLeft() ? 0 : width() - Size::Hairline, 0, Size::Hairline, height() ),
                t->ink( InkAlpha::Divider ) );
}
