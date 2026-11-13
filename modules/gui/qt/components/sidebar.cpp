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
#include "menus.hpp"                    /* MENU_KEY_* */
#include "util/theme.hpp"
#include "util/customwidgets.hpp"     /* withHotkey */
#include "util/redesign_icons.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QtMath>
#include <QStyle>
#include <QVBoxLayout>

using namespace RedesignTokens;

static_assert( Size::SidebarRailBelow == Breakpoint::M,
               "the sidebar turns into a rail at the M breakpoint" );

/* One sidebar entry, painted from tokens: the stylesheet cannot left-align
 * a tool button's content nor set the 12 px icon-label gap. Open is the
 * accent "Open media" button, Nav a library item, Section a preferences
 * section (tinted as a library item when active, no bar: the owner's
 * call), Back the preferences' back row. */
class SidebarButton : public QAbstractButton
{
public:
    enum Kind { Open, Nav, Section, Back };

    SidebarButton( const QString &label, const char *_icon, Kind _kind,
                   QWidget *parent )
        : QAbstractButton( parent ), icon( _icon ), kind( _kind ), rail( false )
    {
        setText( label );
        setAccessibleName( label );
        /* Tab only: a click must not take the keys from the player */
        setFocusPolicy( Qt::TabFocus );
        setAttribute( Qt::WA_Hover );
    }

    /* rail: the square sizeHint, centred, so Open and the highlights keep
     * their shape at every rail width; expanded: the whole row */
    void setRail( bool b )
    {
        rail = b;
        if( parentWidget() && parentWidget()->layout() )
            parentWidget()->layout()->setAlignment( this, b ? Qt::AlignHCenter : Qt::Alignment() );
        update();
    }

    QSize sizeHint() const Q_DECL_OVERRIDE
    {
        const Theme *t = Theme::instance();
        const int h = kind == Open ? t->metric( Theme::SidebarOpenBtnH )
                    : kind == Back ? t->px( Size::SidebarBackH )
                                   : t->metric( Theme::SidebarItemH );
        return QSize( h, h );
    }

protected:
    /* Home and End go to the first and last section, as in a list */
    void keyPressEvent( QKeyEvent *e ) Q_DECL_OVERRIDE
    {
        QButtonGroup *g = group();
        if( kind == Section && g && e->modifiers() == Qt::NoModifier
         && ( e->key() == Qt::Key_Home || e->key() == Qt::Key_End ) )
        {
            QAbstractButton *b = e->key() == Qt::Key_Home ? g->buttons().first()
                                                          : g->buttons().last();
            b->setFocus( Qt::TabFocusReason );
            b->click();
            return;
        }
        QAbstractButton::keyPressEvent( e );
    }

    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    const char *icon;
    Kind kind;
    bool rail;
};

void SidebarButton::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    const bool primary = kind == Open;
    const int radius = primary ? t->px( Size::SidebarOpenRadius ) : t->metric( Theme::RadiusMd );

    /* the accent button's ring goes outside its fill, a gap of the
     * sidebar between: a ring on the accent itself is too faint */
    const qreal ringOut = primary && Theme::showsFocus( this ) ? 2 * Size::FocusRing : 0;
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
    p.drawRoundedRect( QRectF( rect() ).adjusted( ringOut, ringOut, -ringOut, -ringOut ),
                       radius - ringOut, radius - ringOut );

    if( Theme::showsFocus( this ) )
        Theme::paintFocusRing( &p, rect(), radius - Size::FocusRing / 2. );

    const Theme::ColorRole ink = primary ? Theme::OnAccent
                               : isChecked() ? Theme::AccentText
                               : kind == Back && underMouse() ? Theme::Text : Theme::Text2;
    QFont font = t->font( Theme::Body );
    if( primary )
        font.setWeight( QFont::DemiBold );
    const QFontMetrics fm( font );

    /* rail: the icon alone, centred; primary: icon and label centred as a
     * group; nav item: icon and label from the left */
    const int iconPx = t->px( Size::SidebarIcon );
    const int gap = t->px( primary ? Size::SidebarOpenIconGap
                         : kind == Back ? Size::SidebarBackIconGap : Size::SidebarIconGap );
    const int padX = t->px( Size::SidebarItemPadX );
    /* QIcon::paint picks the device-pixel size: render the tint at that size */
    const int devicePx = qRound( iconPx * devicePixelRatioF() );
    const QIcon glyph = t->icon( QLatin1String( icon ), ink, QSize( devicePx, devicePx ) );
    QImage glyphImage = glyph.pixmap( devicePx, devicePx ).toImage();
    /* the glyph's drawn part: the + fills only the middle of its box, and
     * the box's empty sides read as extra space before it and before the
     * label (#11). The accent button spaces and centres what is drawn. */
    int inkLeft = 0, inkW = iconPx;
    if( primary && !rail )
    {
        const QImage scan = glyphImage.convertToFormat( QImage::Format_ARGB32 );
        int l = scan.width(), r = -1;
        for( int y = 0; y < scan.height(); y++ )
        {
            const QRgb *line = reinterpret_cast<const QRgb *>( scan.constScanLine( y ) );
            for( int gx = 0; gx < scan.width(); gx++ )
                if( qAlpha( line[gx] ) > 24 )
                {
                    l = qMin( l, gx ); r = qMax( r, gx );
                }
        }
        if( r >= l )
        {
            const qreal dprI = devicePixelRatioF();
            inkLeft = qFloor( l / dprI );
            inkW = qCeil( ( r + 1 ) / dprI ) - inkLeft;
        }
    }
    int x;
    if( rail )
        x = ( width() - iconPx ) / 2;
    else if( primary )
        x = ( width() - inkW - gap - fm.width( text() ) ) / 2 - inkLeft;
    else
        x = padX;
    const QRect iconRect( x, ( height() - iconPx ) / 2, iconPx, iconPx );
    const QRect iconAt = QStyle::visualRect( layoutDirection(), rect(), iconRect );
    if( kind == Back && isRightToLeft() )
        /* the arrow points the way back: to the right in RTL */
        glyphImage = glyphImage.mirrored( true, false );
    /* placed in device pixels round its box's centre (the rail: the
     * square's): at a fractional scale (1.2) the logical box and Qt's own
     * rounding put the glyph a pixel or two off */
    const qreal dpr = devicePixelRatioF();
    QPointF centre = ( rail ? QRectF( rect() ) : QRectF( iconAt ) ).center();
    if( primary && !rail )
    {
        /* the + on the label's optical middle, between its capitals' and
         * its lowercase letters' middles: on the box's middle it sat above
         * a mostly lowercase label (#10) */
        const QFontMetricsF fmf( font );
        const qreal baseline = ( height() - fmf.height() ) / 2 + fmf.ascent() - t->textNudge( font );
        centre.setY( baseline - ( fmf.tightBoundingRect( QStringLiteral( "H" ) ).height()
                                  + fmf.xHeight() ) / 4 );
    }
    centre *= dpr;
    p.save();
    p.scale( 1 / dpr, 1 / dpr );
    p.drawImage( QRect( qRound( centre.x() - devicePx / 2. ), qRound( centre.y() - devicePx / 2. ),
                        devicePx, devicePx ), glyphImage );
    p.restore();
    if( rail )
        return;

    const int textX = primary ? x + inkLeft + inkW + gap : iconRect.right() + 1 + gap;
    const QRect textRect( textX, 0, width() - textX - padX, height() );
    p.setFont( font );
    p.setPen( t->color( ink ) );
    const QRect textAt = QStyle::visualRect( layoutDirection(), rect(), textRect ).translated( 0, -t->textNudge( p.font() ) );
    const QString label = fm.elidedText( text(), Qt::ElideRight, textRect.width() );
    if( primary )   /* on the accent: grayscale, no ClearType fringes */
        Theme::drawGrayText( &p, textAt, Qt::AlignVCenter | Qt::AlignLeft, label, t->color( ink ) );
    else
        p.drawText( textAt, Qt::AlignVCenter | Qt::AlignLeft, label );
}

/* The sidebar's padding and gap, again when the scale changes */
static void padNav( QLayout *layout, int spacing )
{
    const Theme *t = Theme::instance();
    layout->setContentsMargins( t->px( Size::SidebarPadX ), t->px( Size::SidebarPadY ),
                                t->px( Size::SidebarPadX ), t->px( Size::SidebarPadY ) );
    layout->setSpacing( t->px( spacing ) );
    layout->invalidate();   /* also for a resized gap (gapBelow) */
}

/* A gap of the token's height, resized with gapBelow() again when the scale changes */
static QSpacerItem *gapBelow( QSpacerItem *gap, int token )
{
    const int h = Theme::instance()->px( token );
    if( !gap )
        return new QSpacerItem( 0, h, QSizePolicy::Minimum, QSizePolicy::Fixed );
    gap->changeSize( 0, h, QSizePolicy::Minimum, QSizePolicy::Fixed );
    return gap;
}

/* A page of the sidebar's stack, with the sidebar's padding */
static QVBoxLayout *navLayout( QStackedWidget *navs, int spacing )
{
    QWidget *nav = new QWidget;
    navs->addWidget( nav );
    QVBoxLayout *layout = new QVBoxLayout( nav );
    padNav( layout, spacing );
    return layout;
}

Sidebar::Sidebar( intf_thread_t *p_intf, QWidget *parent )
    : QWidget( parent ), currentPage( NULL ), returnPage( NULL )
{
    setAccessibleName( qtr( "Library" ) );
    QVBoxLayout *outer = new QVBoxLayout( this );
    outer->setContentsMargins( 0, 0, 0, 0 );
    navs = new QStackedWidget( this );
    outer->addWidget( navs );
    QVBoxLayout *layout = navLayout( navs, Size::SidebarItemGap );
    const Theme *t = Theme::instance();

    SidebarButton *open = new SidebarButton( qtr( "Open media" ), RedesignIcons::IconOpenMedia,
                                             SidebarButton::Open, this );
    open->setToolTip( withHotkey( p_intf, qtr( "Open media" ), MENU_KEY_OPEN_MEDIA ) );
    connect( open, &QAbstractButton::clicked, this, []() { THEDP->openDialog(); } );
    layout->addWidget( open );
    openGap = gapBelow( NULL, Size::SidebarOpenBelow );
    layout->addSpacerItem( openGap );
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
        }
        SidebarButton *b = new SidebarButton( label, nav[i].icon, SidebarButton::Nav, this );
        b->setCheckable( true );
        b->setAutoExclusive( true ); /* screen readers: a radio button; the group rules */
        b->setToolTip( label );
        group->addButton( b, i );
        /* toggled, not clicked: screen readers' activation toggles */
        connect( b, &QAbstractButton::toggled, this,
                 [this, i]( bool on ) { if( on ) emit pageRequested( pages[i] ); } );
        layout->addWidget( b );
        buttons.append( b );
    }

    layout->addStretch( 1 );
    SidebarButton *prefs = new SidebarButton( qtr( "Preferences" ), RedesignIcons::IconSettings,
                                              SidebarButton::Nav, this );
    prefs->setToolTip( withHotkey( p_intf, qtr( "Preferences" ), MENU_KEY_PREFERENCES ) );
    prefs->setCheckable( true );
    prefs->setAutoExclusive( true );
    group->addButton( prefs, Preferences );
    /* the menu's way, which opens the complete tree in a dialog when it is
     * asked for: then the item the click checked goes back */
    connect( prefs, &QAbstractButton::toggled, this, [this]( bool on ) {
        if( !on )
            return;
        THEDP->prefsDialog();
        setCurrentWidget( currentPage );
    } );
    layout->addWidget( prefs );
    buttons.append( prefs );

    /* DESIGN_SPEC 8: the preferences navigation, back, the heading and the
     * sections; the page shows the section the sidebar picks */
    QVBoxLayout *prefsNav = navLayout( navs, Size::SidebarSectionGap );
    prefsNav->parentWidget()->setAccessibleName( qtr( "Preferences sections" ) );
    back = new SidebarButton( QString(), RedesignIcons::IconBack, SidebarButton::Back, this );
    setBackLabel( true );
    connect( back, &QAbstractButton::clicked, this, &Sidebar::goBack );
    prefsNav->addWidget( back );
    backGap = gapBelow( NULL, Size::SidebarBackBelow );
    prefsNav->addSpacerItem( backGap );
    buttons.append( back );
    heading = new QLabel( qtr( "Preferences" ) );
    QFont font = t->font( Theme::NavTitle );
    font.setLetterSpacing( QFont::PercentageSpacing, Type::TitleTracking );
    heading->setFont( font );   /* its padding: setBreakpoint() */
    prefsNav->addWidget( heading );
    sections = new QButtonGroup( this );
    SidebarButton *section = NULL;
    for( int i = 0; i < PrefsPage::SectionCount; i++ )
    {
        const QString label = qtr( PrefsPage::sections[i].label );
        section = new SidebarButton( label, PrefsPage::sections[i].icon,
                                     SidebarButton::Section, this );
        section->setCheckable( true );
        section->setAutoExclusive( true );
        section->setChecked( i == SPrefsDefaultCat );
        section->setToolTip( label );
        sections->addButton( section, i );
        connect( section, &QAbstractButton::toggled, this,
                 [this, i]( bool on ) { if( on ) prefsPage->showSection( i ); } );
        prefsNav->addWidget( section );
        buttons.append( section );
    }
    prefsNav->addStretch( 1 );
    /* Tab goes from the last section into the page */
    pages[Preferences] = prefsPage = new PrefsPage( p_intf, section );
    connect( prefsPage, &PrefsPage::closeRequested, this, &Sidebar::goBack );

    connect( Theme::instance(), &Theme::changed, this, [this]() { update(); } );
}

void Sidebar::showPage( int i )
{
    if( i >= 0 && i < PageCount )
        emit pageRequested( pages[i] );
}

void Sidebar::setCurrentWidget( QWidget *w )
{
    const bool entering = w == pages[Preferences] && currentPage != w;
    if( entering )
        returnPage = currentPage;   /* the way back */
    navs->setCurrentIndex( w == pages[Preferences] ? 1 : 0 );
    if( entering )
        /* the keys start on the section shown */
        sections->checkedButton()->setFocus( Qt::OtherFocusReason );
    currentPage = w;
    for( int i = 0; i < PageCount; i++ )
        if( pages[i] == w )
        {
            /* following the stack, not asking it */
            const QSignalBlocker blocker( group->button( i ) );
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

void Sidebar::goBack()
{
    /* first away, so the page drops its panels without building them again */
    emit pageRequested( returnPage ? returnPage : pages[Home] );
    prefsPage->cancel();
    /* as at startup: Space is play/pause again, not a stray button's */
    window()->setFocus( Qt::OtherFocusReason );
}

void Sidebar::setBackLabel( bool library )
{
    const QString label = library ? qtr( "Back to library" ) : qtr( "Back to player" );
    back->setText( label );
    back->setAccessibleName( label );
    back->setToolTip( withHotkey( NULL, label, "Esc" ) );
}

int Sidebar::widthAt( BreakpointWatcher::Size bp )
{
    static const int widths[] = { Size::SidebarXS, Size::SidebarS, Size::SidebarM, Size::SidebarL };
    return Theme::instance()->px( widths[bp] );
}

void Sidebar::setBreakpoint( BreakpointWatcher::Size bp )
{
    /* also after a change of scale (BreakpointWatcher) */
    gapBelow( openGap, Size::SidebarOpenBelow );
    gapBelow( backGap, Size::SidebarBackBelow );
    padNav( navs->widget( 0 )->layout(), Size::SidebarItemGap );
    padNav( navs->widget( 1 )->layout(), Size::SidebarSectionGap );
    setFixedWidth( widthAt( bp ) );
    for( SidebarButton *b : buttons )
        b->setRail( bp < BreakpointWatcher::M );
    const Theme *t = Theme::instance();
    heading->setContentsMargins( t->px( Size::SidebarItemPadX ), t->px( Size::SidebarTitlePadTop ),
                                 t->px( Size::SidebarItemPadX ), t->px( Size::SidebarTitlePadBottom ) );
    heading->setVisible( bp >= BreakpointWatcher::M );
    prefsPage->setNarrow( bp == BreakpointWatcher::XS );
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
