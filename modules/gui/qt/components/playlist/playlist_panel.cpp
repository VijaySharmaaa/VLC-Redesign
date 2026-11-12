/*****************************************************************************
 * playlist_panel.cpp : the docked playlist beside the video
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

#include "components/playlist/playlist_panel.hpp"
#include "components/playlist/playlist_model.hpp"   /* PLModel */
#include "components/playlist/standardpanel.hpp"    /* context menu entries */
#include "components/playlist/views.hpp"            /* PlListView */
#include "components/playlist/sorting.h"            /* COLUMN_* */
#include "components/library/library_widgets.hpp"     /* setupSearchField */
#include "components/overlay_host.hpp"
#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include "menus.hpp"                    /* MENU_KEY_* */
#include "util/customwidgets.hpp"                   /* timeString */
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_intf_strings.h>                       /* I_PL_ADDF */
#include <vlc_input_item.h>                         /* input_item_GetDuration */

#include <QApplication>
#include <QBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVariantAnimation>

using namespace RedesignTokens;

namespace
{

/* One 40 px row: number or now-playing glyph, title over meta, duration.
 * Text only from the item's meta: no cover art is read while painting. */
class RowDelegate : public QStyledItemDelegate
{
public:
    RowDelegate( intf_thread_t *_p_intf, QObject *parent )
        : QStyledItemDelegate( parent ), p_intf( _p_intf ) {}

    QSize sizeHint( const QStyleOptionViewItem &, const QModelIndex & ) const Q_DECL_OVERRIDE
    {
        return QSize( 0, Theme::instance()->metric( Theme::PlaylistRowH ) );
    }

    void paint( QPainter *, const QStyleOptionViewItem &, const QModelIndex & ) const Q_DECL_OVERRIDE;

private:
    intf_thread_t *p_intf;
};

void RowDelegate::paint( QPainter *p, const QStyleOptionViewItem &option,
                         const QModelIndex &index ) const
{
    Theme *t = Theme::instance();
    const QRect r = option.rect;
    const bool current = index.data( VLCModelSubInterface::CURRENT_ITEM_ROLE ).toBool();
    const int radius = t->metric( Theme::RadiusMd );

    p->save();
    p->setRenderHint( QPainter::Antialiasing );
    p->setPen( Qt::NoPen );
    if( current )
    {
        p->setBrush( t->color( Theme::AccentTint12 ) );
        p->drawRoundedRect( r, radius, radius );
    }
    if( option.state & ( QStyle::State_Selected | QStyle::State_MouseOver ) )
    {
        p->setBrush( t->ink( ( option.state & QStyle::State_Selected ) ? InkAlpha::OpenFill
                                                                      : InkAlpha::Hairline ) );
        p->drawRoundedRect( r, radius, radius );
    }
    if( ( option.state & QStyle::State_HasFocus ) && Theme::showsFocus( option.widget ) )
        Theme::paintFocusRing( p, r, radius - Size::FocusRing / 2. );

    const Qt::LayoutDirection dir = option.direction;
    const int padX = t->px( Playlist::RowPadX );
    const int gap = t->px( Playlist::RowGap );
    const QRect numRect( r.x() + padX, r.y(), t->px( Playlist::RowNumberW ), r.height() );
    if( current )
    {
        const bool paused = THEMIM->getIM()->playingStatus() == PAUSE_S;
        const int iconPx = t->px( Playlist::RowIcon );
        const qreal dpr = option.widget ? option.widget->devicePixelRatioF() : 1.;
        const int devicePx = qRound( iconPx * dpr );
        t->icon( QLatin1String( paused ? RedesignIcons::IconPause : RedesignIcons::IconNowPlaying ),
                 Theme::Accent, QSize( devicePx, devicePx ) )
            .paint( p, QStyle::visualRect( dir, r, numRect ) );
    }
    else
    {
        QFont numFont = t->font( Theme::Meta );
        numFont.setWeight( QFont::Medium );
        p->setFont( numFont );
        /* Dim is decoration only; on the selected fill the number must read */
        p->setPen( t->color( ( option.state & QStyle::State_Selected ) ? Theme::Mute : Theme::Dim ) );
        p->drawText( QStyle::visualRect( dir, r, numRect ), Qt::AlignCenter,
                     QString::number( index.row() + 1 ) );
    }

    const QFont durFont = t->font( Theme::Meta );
    const QString duration = VLCModel::getMeta( index, COLUMN_DURATION );
    const int durW = QFontMetrics( durFont ).width( duration );
    const QRect durRect( r.right() + 1 - padX - durW, r.y(), durW, r.height() );
    p->setFont( durFont );
    p->setPen( t->color( Theme::Mute ) );
    p->drawText( QStyle::visualRect( dir, r, durRect ), Qt::AlignVCenter | Qt::AlignRight, duration );

    const int textX = numRect.right() + 1 + gap;
    const int textW = ( duration.isEmpty() ? r.right() + 1 - padX : durRect.left() - gap ) - textX;
    QFont titleFont = t->font( Theme::Body );
    titleFont.setWeight( QFont::Medium );
    QFont metaFont = t->font( Theme::Overline );   /* 11 px, regular */
    metaFont.setWeight( QFont::Normal );
    const QFontMetrics titleFm( titleFont ), metaFm( metaFont );

    QStringList metaParts;
    foreach( int column, QList<int>() << COLUMN_ARTIST << COLUMN_ALBUM )
    {
        const QString part = VLCModel::getMeta( index, column );
        if( !part.isEmpty() )
            metaParts << part;
    }
    const QString meta = metaParts.join( QString::fromUtf8( " \xC2\xB7 " ) );
    const int metaGap = t->px( Playlist::RowMetaGap );
    const int blockH = titleFm.height() + ( meta.isEmpty() ? 0 : metaGap + metaFm.height() );
    const int top = r.y() + ( r.height() - blockH ) / 2;

    const QRect titleRect( textX, top, textW, titleFm.height() );
    p->setFont( titleFont );
    p->setPen( t->color( current ? Theme::AccentText : Theme::Text ) );
    p->drawText( QStyle::visualRect( dir, r, titleRect ), Qt::AlignVCenter | Qt::AlignLeft,
                 titleFm.elidedText( index.data().toString(), Qt::ElideRight, textW ) );
    if( !meta.isEmpty() )
    {
        const QRect metaRect( textX, titleRect.bottom() + 1 + metaGap, textW, metaFm.height() );
        p->setFont( metaFont );
        p->setPen( t->color( Theme::Mute ) );
        p->drawText( QStyle::visualRect( dir, r, metaRect ), Qt::AlignVCenter | Qt::AlignLeft,
                     metaFm.elidedText( meta, Qt::ElideRight, textW ) );
    }
    p->restore();
}

} // namespace

PlaylistPanel::PlaylistPanel( intf_thread_t *_p_intf, QBoxLayout *_dock, QWidget *over )
    : QWidget( _dock->parentWidget() ), p_intf( _p_intf ), dock( _dock ),
      dockParent( _dock->parentWidget() ), fullWidth( 0 ), shownWidth( 0 ), drawer( false )
{
    setObjectName( "playlistPanel" );
    setAccessibleName( qtr( "Playlist" ) );
    /* the drawer's shadow is see-through; paintEvent fills the panel */
    setAttribute( Qt::WA_TranslucentBackground );
    hide();
    dock->addWidget( this );

    Theme *t = Theme::instance();
    model = PLModel::getPLModel( p_intf );

    content = new QWidget( this );
    QVBoxLayout *layout = new QVBoxLayout( content );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );

    /* Header: title and count, add, close */
    QHBoxLayout *head = new QHBoxLayout;
    head->setContentsMargins( t->px( Playlist::HeadPadLeft ), t->px( Playlist::HeadPadTop ),
                              t->px( Playlist::HeadPadRight ), t->px( Playlist::HeadPadBottom ) );
    head->setSpacing( t->px( Playlist::HeadGap ) );
    QVBoxLayout *titles = new QVBoxLayout;
    titles->setSpacing( t->px( Playlist::CountGap ) );
    QLabel *title = new QLabel( qtr( "Playlist" ), content );
    title->setFont( t->font( Theme::PanelTitle ) );
    count = new QLabel( content );
    count->setProperty( "role", "hint" );
    count->setFont( t->font( Theme::Meta ) );
    titles->addWidget( title );
    titles->addWidget( count );
    head->addLayout( titles, 1 );

    const int buttonPx = t->px( Playlist::HeadButton );
    const int glyphPx = t->px( Playlist::HeadButtonGlyph );
    QToolButton *add = new QToolButton( content );
    QToolButton *close = new QToolButton( content );
    const QString closeTip = withHotkey( p_intf, qtr( "Close playlist" ), MENU_KEY_PLAYLIST );
    const struct { QToolButton *button; const char *icon; QString name, tip; } buttons[] = {
        { add, RedesignIcons::IconAdd, qtr( I_PL_ADDF ), qtr( I_PL_ADDF ) },
        { close, RedesignIcons::IconClose, qtr( "Close playlist" ), closeTip },
    };
    for( size_t i = 0; i < sizeof( buttons ) / sizeof( buttons[0] ); i++ )
    {
        QToolButton *b = buttons[i].button;
        b->setIcon( Theme::themedIcon( buttons[i].icon, Theme::Text2 ) );
        b->setIconSize( QSize( glyphPx, glyphPx ) );
        b->setFixedSize( buttonPx, buttonPx );
        b->setAccessibleName( buttons[i].name );
        b->setToolTip( buttons[i].tip );
        head->addWidget( b );
    }
    connect( add, &QToolButton::clicked, this, []() { THEDP->PLAppendDialog(); } );
    connect( close, &QToolButton::clicked, this, &PlaylistPanel::closeRequested );
    layout->addLayout( head );

    /* Filter: filters the shared model, so the separate window's views too */
    /* as the library's search field */
    QLineEdit *filter = new QLineEdit( content );
    filter->setAccessibleName( qtr( "Filter playlist" ) );
    filter->setPlaceholderText( qtr( "Filter playlist" ) );
    filter->setToolTip( qtr( "Filter playlist" ) );
    setupSearchField( filter, qtr( "Filter" ), qtr( "Clear filter" ) );
    filter->setFixedHeight( t->metric( Theme::SelectH ) );
    connect( filter, &QLineEdit::textChanged, this,
             [this]( const QString &text ) { model->filter( text, QModelIndex(), true ); } );
    QHBoxLayout *filterRow = new QHBoxLayout;
    filterRow->setContentsMargins( t->px( Playlist::FilterMarginX ), 0,
                                   t->px( Playlist::FilterMarginX ), t->px( Playlist::FilterMarginBottom ) );
    filterRow->addWidget( filter );
    layout->addLayout( filterRow );

    /* Items: PlListView brings the model's drag and drop and keeps Space
     * for the player; uniform rows keep 10,000 items cheap */
    view = new PlListView( model, content );
    view->setModel( model );
    view->setItemDelegate( new RowDelegate( p_intf, view ) );
    view->setAlternatingRowColors( false );
    view->setFrameShape( QFrame::NoFrame );
    view->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    view->setAccessibleName( qtr( "Playlist items" ) );
    view->setContextMenuPolicy( Qt::CustomContextMenu );
    view->installEventFilter( this );
    connect( view, &PlListView::activated, model,
             static_cast<void (PLModel::*)( const QModelIndex & )>( &PLModel::activateItem ) );
    connect( view, &PlListView::customContextMenuRequested, this, &PlaylistPanel::popup );
    QHBoxLayout *listRow = new QHBoxLayout;
    listRow->setContentsMargins( t->px( Playlist::ListPadX ), 0,
                                 t->px( Playlist::ListPadX ), t->px( Playlist::ListPadBottom ) );
    listRow->addWidget( view );
    layout->addLayout( listRow, 1 );

    connect( model, &VLCModel::currentIndexChanged, this, [this]( const QModelIndex &index ) {
        if( isVisible() )
            view->scrollTo( index );
    } );
    connect( THEMIM->getIM(), &InputManager::playingStatusChanged,
             view->viewport(), static_cast<void (QWidget::*)()>( &QWidget::update ) );

    /* one count per burst of model changes (a 10,000 item load) */
    countTimer.setSingleShot( true );
    countTimer.setInterval( 0 );
    connect( &countTimer, &QTimer::timeout, this, [this]() {
        const int n = model->rowCount();
        QString text = QString::fromUtf8( vlc_ngettext( "%1 item", "%1 items", n ) ).arg( n );
        /* the known durations' sum; unknown ones (streams) count nothing */
        mtime_t total = 0;
        for( int i = 0; i < n; i++ )
        {
            input_item_t *item = model->getInputItem( model->index( i, 0, QModelIndex() ) );
            const mtime_t duration = item ? input_item_GetDuration( item ) : 0;
            if( duration > 0 )
                total += duration;
        }
        if( total > 0 )
            text += QString::fromUtf8( " · " ) + timeString( total / CLOCK_FREQ );
        count->setText( text );
    } );
    connect( model, &QAbstractItemModel::rowsInserted, this, [this]() { countTimer.start(); } );
    connect( model, &QAbstractItemModel::rowsRemoved, this, [this]() { countTimer.start(); } );
    connect( model, &QAbstractItemModel::modelReset, this, [this]() { countTimer.start(); } );
    /* durations arrive with preparsing */
    connect( model, &QAbstractItemModel::dataChanged, this, [this]() { countTimer.start(); } );
    countTimer.start();

    host = new OverlayHost( over, this );
    connect( host, &OverlayHost::changed, this, &PlaylistPanel::place );

    slide = new QVariantAnimation( this );
    slide->setEasingCurve( OverlayWindow::enterCurve() );
    connect( slide, &QVariantAnimation::valueChanged, this,
             [this]( const QVariant &v ) { setShownWidth( v.toInt() ); } );

    connect( t, &Theme::changed, this, [this]() { update(); view->viewport()->update(); } );
}

void PlaylistPanel::setOpen( bool open )
{
    if( open == isVisible() )
        return;
    slide->stop();
    if( !open )
    {
        hide();   /* exit is instant */
        return;
    }
    const int ms = Theme::instance()->duration( Theme::PanelSlide );
    setShownWidth( ms ? 0 : fullWidth );
    show();
    view->scrollTo( model->currentIndex() );
    if( drawer )
    {
        /* a window of its own: take the keys, Esc gives them back */
        raise();
        activateWindow();
        view->setFocus();
    }
    if( ms )
    {
        slide->setDuration( ms );
        slide->setStartValue( 0 );
        slide->setEndValue( fullWidth );
        slide->start();
    }
}

void PlaylistPanel::setBreakpoint( BreakpointWatcher::Size bp )
{
    static const Theme::SizeRole widths[] = {
        Theme::PlaylistS, Theme::PlaylistS, Theme::PlaylistM, Theme::PlaylistL };
    fullWidth = Theme::instance()->metric( widths[bp] );

    const bool shown = isVisible();
    if( ( bp == BreakpointWatcher::XS ) != drawer )
    {
        /* the panel moves, never the video: its native window stays put */
        drawer = bp == BreakpointWatcher::XS;
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
}

void PlaylistPanel::setShownWidth( int w )
{
    shownWidth = w;
    if( drawer )
        place();
    else
        setFixedWidth( w );
}

int PlaylistPanel::shadowWidth() const
{
    return drawer ? Theme::instance()->px( Playlist::DrawerShadow ) : 0;
}

/* The drawer: over the right of the target, the shadow to its left */
void PlaylistPanel::place()
{
    if( !drawer )
        return;
    const QRect target = host->targetRect();
    const int w = shownWidth + shadowWidth();
    /* shortcut: always on the right, as in a left-to-right layout; mirror
     * it with the dock once right-to-left layouts are checked */
    setGeometry( target.right() + 1 - w, target.top(), w, target.height() );
}

void PlaylistPanel::resizeEvent( QResizeEvent * )
{
    content->setGeometry( shadowWidth(), 0, fullWidth, height() );
}

void PlaylistPanel::paintEvent( QPaintEvent * )
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
    p.fillRect( s, 0, width() - s, height(), t->color( Theme::Panel ) );
    /* the hairline towards the content */
    p.fillRect( s, 0, Size::Hairline, height(), t->ink( InkAlpha::Divider ) );
}

void PlaylistPanel::keyPressEvent( QKeyEvent *e )
{
    if( drawer && e->key() == Qt::Key_Escape )
        emit closeRequested();
    else if( drawer && dockParent->window() )
        /* a window of its own: the player's hotkeys still apply */
        QApplication::sendEvent( dockParent->window(), e );
    else
        QWidget::keyPressEvent( e );
}

bool PlaylistPanel::eventFilter( QObject *obj, QEvent *e )
{
    if( obj == view && e->type() == QEvent::KeyPress )
    {
        const int key = static_cast<QKeyEvent *>( e )->key();
        if( key == Qt::Key_Delete || key == Qt::Key_Backspace )
        {
            model->doDelete( view->selectionModel()->selectedIndexes() );
            return true;
        }
    }
    return QWidget::eventFilter( obj, e );
}

void PlaylistPanel::popup( const QPoint &pos )
{
    const QModelIndex clicked = view->indexAt( pos );
    if( clicked.isValid() && !view->selectionModel()->isSelected( clicked ) )
        view->setCurrentIndex( clicked );
    /* the first selected row, read again once the menu closes: the playlist
     * may change while it is open */
    const auto first = [this]() {
        const QModelIndexList rows = view->selectionModel()->selectedRows();
        return rows.isEmpty() ? QModelIndex() : rows.first();
    };

    QMenu menu;
    StandardPLPanel::addModelActions( &menu, model, first() );
    if( menu.isEmpty() )
        return;
    QAction *action = menu.exec( view->viewport()->mapToGlobal( pos ) );
    if( action )
        StandardPLPanel::runModelAction( p_intf, this, model, action,
                                         view->selectionModel()->selectedRows(), first() );
}
