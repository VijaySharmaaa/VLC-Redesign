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
#include "input_manager.hpp"
#include "components/playlist/playlist.hpp"         /* PlaylistWidget::enqueueDrop */
#include "dialogs_provider.hpp"
#include "menus.hpp"                    /* MENU_KEY_* */
#include "util/customwidgets.hpp"                   /* timeString */
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_intf_strings.h>                       /* I_PL_ADDF */
#include <vlc_input_item.h>                         /* input_item_GetDuration */

#include <QApplication>
#include <QBoxLayout>
#include <QDropEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
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
    metaFont.setCapitalization( QFont::MixedCase );
    const QFontMetrics titleFm( titleFont ), metaFm( metaFont );

    QStringList metaParts;
    foreach( int column, QList<int>() << COLUMN_ARTIST << COLUMN_ALBUM )
    {
        const QString part = VLCModel::getMeta( index, column );
        if( !part.isEmpty() )
            metaParts << part;
    }
    const QString meta = metaParts.join( metaSeparator() );
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
    : SidePanel( _dock, over ), p_intf( _p_intf )
{
    setObjectName( "playlistPanel" );
    setAccessibleName( qtr( "Playlist" ) );

    Theme *t = Theme::instance();
    /* not the shared model: the playlist window's sources (My Videos...)
     * rebuild that one on their own node */
    playlist_Lock( THEPL );
    playlist_item_t *root = THEPL->p_playing;
    playlist_Unlock( THEPL );
    model = new PLModel( THEPL, p_intf, root, this );

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

    /* Filter, as the library's search field. It marks the playlist's items
     * (PLModel::filter), so the playlist window shows the same search. */
    filter = new QLineEdit( content );
    filter->setAccessibleName( qtr( "Filter playlist" ) );
    filter->setPlaceholderText( qtr( "Filter playlist" ) );
    filter->setToolTip( qtr( "Filter playlist" ) );
    setupSearchField( filter, qtr( "Filter" ), qtr( "Clear filter" ) );
    filter->setFixedHeight( t->metric( Theme::SelectH ) );
    connect( filter, &QLineEdit::textChanged, this, [this]( const QString &text ) {
        /* not when the shared model's search set this text */
        if( !model->mainFiltered( text ) )
            model->filter( text, QModelIndex(), true );
    } );
    /* the shared model (the playlist window, audio mode's tracks) and this
     * one show the same order and filter, while it shows the main playlist */
    PLModel *shared = PLModel::getPLModel( p_intf );
    connect( model, &PLModel::mainEdited, shared, &PLModel::followMain );
    connect( shared, &PLModel::mainEdited, model, &PLModel::followMain );
    connect( shared, &PLModel::searchChanged, filter, [this]( const QString &text ) {
        if( filter->text() != text )
            filter->setText( text );
    } );
    /* a search made in the playlist window before this opened */
    filter->setText( PLModel::mainSearch() );
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

    /* one count per burst of model changes (a 10,000 item load, each
     * item's preparse), and none while closed: setOpen() counts */
    countTimer.setSingleShot( true );
    countTimer.setInterval( Playlist::CountDebounceMs );
    connect( &countTimer, &QTimer::timeout, this, [this]() {
        if( isVisible() )
            updateCount();
    } );
    connect( model, &QAbstractItemModel::rowsInserted, this, [this]() { countTimer.start(); } );
    connect( model, &QAbstractItemModel::rowsRemoved, this, [this]() { countTimer.start(); } );
    connect( model, &QAbstractItemModel::modelReset, this, [this]() { countTimer.start(); } );
    /* PLModel::followMain(): the other view filtered or sorted */
    connect( model, &QAbstractItemModel::layoutChanged, this, [this]() { countTimer.start(); } );
    /* durations arrive with preparsing */
    connect( model, &QAbstractItemModel::dataChanged, this, [this]() { countTimer.start(); } );

    connect( t, &Theme::changed, this, [this]() { update(); view->viewport()->update(); } );

    /* the list takes its own reordering drags, other drops come here */
    setAcceptDrops( true );
}

void PlaylistPanel::updateCount()
{
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
        text += metaSeparator() + timeString( total / CLOCK_FREQ );
    count->setText( text );
}

void PlaylistPanel::setOpen( bool open )
{
    if( open == isVisible() )
        return;
    if( !open )
    {
        slide->stop();
        hide();   /* exit is instant */
        return;
    }
    updateCount();
    slideIn();
    show();
    view->scrollTo( model->currentIndex() );
    if( drawer )
    {
        /* a window of its own: take the keys, Esc gives them back */
        raise();
        activateWindow();
        view->setFocus();
    }
}

void PlaylistPanel::focusFilter()
{
    if( drawer )
    {
        raise();
        activateWindow();
    }
    filter->setFocus( Qt::ShortcutFocusReason );
}

void PlaylistPanel::clearFilter()
{
    filter->clear();
}

void PlaylistPanel::setBreakpoint( BreakpointWatcher::Size bp )
{
    static const Theme::SizeRole widths[] = {
        Theme::PlaylistS, Theme::PlaylistS, Theme::PlaylistM, Theme::PlaylistL };
    setDocked( bp == BreakpointWatcher::XS, Theme::instance()->metric( widths[bp] ) );
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

void PlaylistPanel::dragEnterEvent( QDragEnterEvent *e )
{
    if( e->mimeData()->hasUrls() )
        e->acceptProposedAction();
}

void PlaylistPanel::dropEvent( QDropEvent *e )
{
    PlaylistWidget::enqueueDrop( p_intf, e );
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
