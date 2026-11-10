/*****************************************************************************
 * network_page.cpp : the Network library page
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

#include "components/library/network_page.hpp"
#include "components/now_playing.hpp"           /* fontAt */
#include "components/playlist/playlist_model.hpp"
#include "dialogs_provider.hpp"
#include "input_manager.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_services_discovery.h>

#include <QApplication>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

using namespace RedesignTokens;

/**********************************************************************
 * NetworkTile
 **********************************************************************/
NetworkTile::NetworkTile( const char *_icon, const QString &name, const QString &_address,
                          QWidget *parent )
    : QAbstractButton( parent ), icon( _icon ), address( _address )
{
    setText( name );
    setAccessibleName( name );
    setAccessibleDescription( address );
    setToolTip( address.isEmpty() ? name : QString( "%1\n%2" ).arg( name, address ) );
    setFocusPolicy( Qt::StrongFocus );
    setAttribute( Qt::WA_Hover );
    /* the grid's column stretch alone sets the width: equal columns */
    setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
}

QSize NetworkTile::sizeHint() const
{
    const Theme *t = Theme::instance();
    const int text = QFontMetrics( fontAt( Theme::Body, Type::Body, true ) ).height()
                   + t->px( Library::TileSubGap ) + QFontMetrics( t->font( Theme::Mono ) ).height();
    return QSize( 0, 2 * t->px( Library::TilePad ) + qMax( t->px( Library::TileIcon ), text ) );
}

void NetworkTile::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    const QRectF r = QRectF( rect() ).adjusted( .5, .5, -.5, -.5 );
    const qreal radius = t->px( Library::TileRadius );
    p.setPen( QPen( t->ink( InkAlpha::Divider ), Size::Hairline ) );
    p.setBrush( t->color( underMouse() ? Theme::Raised : Theme::Surface ) );
    p.drawRoundedRect( r, radius, radius );
    if( hasFocus() )
    {
        const qreal inset = Size::FocusRing / 2.;
        p.setPen( QPen( t->color( t->isDark() ? Theme::Accent : Theme::AccentText ), Size::FocusRing ) );
        p.setBrush( Qt::NoBrush );
        p.drawRoundedRect( r.adjusted( inset, inset, -inset, -inset ), radius - inset, radius - inset );
    }

    const int pad = t->px( Library::TilePad ), iconPx = t->px( Library::TileIcon );
    const QRect iconBox( pad, ( height() - iconPx ) / 2, iconPx, iconPx );
    p.setPen( Qt::NoPen );
    p.setBrush( t->color( Theme::AccentTint12 ) );
    p.drawRoundedRect( iconBox, radius, radius );
    const int glyph = t->px( Library::TileGlyph );
    const int devicePx = qRound( glyph * devicePixelRatioF() );
    t->icon( QLatin1String( icon ), t->isDark() ? Theme::Accent : Theme::AccentText,
             QSize( devicePx, devicePx ) )
        .paint( &p, QRect( iconBox.x() + ( iconPx - glyph ) / 2, iconBox.y() + ( iconPx - glyph ) / 2,
                           glyph, glyph ) );

    const QFont nameFont = fontAt( Theme::Body, Type::Body, true );
    const QFont subFont = t->font( Theme::Mono );
    const QFontMetrics nameFm( nameFont ), subFm( subFont );
    const int x = iconBox.right() + 1 + t->px( Library::TileInnerGap );
    const int w = width() - x - pad;
    const int textH = nameFm.height() + t->px( Library::TileSubGap ) + subFm.height();
    const int y = ( height() - textH ) / 2;
    p.setFont( nameFont );
    p.setPen( t->color( Theme::Text ) );
    p.drawText( QStyle::visualRect( layoutDirection(), rect(), QRect( x, y, w, nameFm.height() ) ),
                Qt::AlignLeft | Qt::AlignVCenter, nameFm.elidedText( text(), Qt::ElideRight, w ) );
    p.setFont( subFont );
    p.setPen( t->color( Theme::Dim ) );
    p.drawText( QStyle::visualRect( layoutDirection(), rect(),
                                    QRect( x, y + textH - subFm.height(), w, subFm.height() ) ),
                Qt::AlignLeft | Qt::AlignVCenter, subFm.elidedText( address, Qt::ElideRight, w ) );
}

void NetworkTile::keyPressEvent( QKeyEvent *e )
{
    if( e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter )
        click();
    else
        QAbstractButton::keyPressEvent( e );
}

/**********************************************************************
 * NetworkPage
 **********************************************************************/
/* the search on the tiles: name or address */
static bool matches( const NetworkTile *tile, const QString &text )
{
    return text.isEmpty() || tile->text().contains( text, Qt::CaseInsensitive )
        || tile->accessibleDescription().contains( text, Qt::CaseInsensitive );
}

/* columns per breakpoint XS, S, M, L */
static const int tileColumns[] = { Library::NetColsXS, Library::NetColsS,
                                   Library::NetColsM, Library::NetColsL };

NetworkPage::NetworkPage( intf_thread_t *_p_intf, QWidget *parent )
    : QWidget( parent ), p_intf( _p_intf ), browsed( NULL ), topIsItem( false ),
      expandingId( -1 ), loaded( false )
{
    const Theme *t = Theme::instance();
    const QString title = qtr( "Network" );
    setAccessibleName( title );
    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setContentsMargins( t->px( Size::PagePadX ), t->px( Size::PagePadTop ),
                                t->px( Size::PagePadX ), t->px( Library::PagePadBottom ) );
    layout->setSpacing( t->px( Library::HeaderBelow ) );
    header = new LibraryHeader( title, this );
    layout->addWidget( header );

    body = new QStackedWidget( this );
    tilesPage = new QScrollArea( body );
    tilesPage->setProperty( "role", "library" );
    tilesPage->setFrameShape( QFrame::NoFrame );
    tilesPage->setWidgetResizable( true );
    tilesPage->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    QWidget *content = new QWidget;
    QVBoxLayout *groups = new QVBoxLayout( content );
    groups->setContentsMargins( 0, 0, 0, 0 );
    groups->setSpacing( t->px( Library::GroupGap ) );
    tilesPage->setWidget( content );
    lanHint = addGroup( qtr( "On your network" ), &lanGrid );
    addGroup( qtr( "Streams" ), &streamGrid )->hide();
    groups->addStretch( 1 );
    body->addWidget( tilesPage );

    filter = new LibraryFilter( this );
    list = new CardView( new CardDelegate( CardDelegate::Row, filter, this ), NULL, body );
    list->setModel( filter );
    body->addWidget( list );
    layout->addWidget( body, 1 );

    rebuild = new QTimer( this );
    rebuild->setSingleShot( true );   /* one rebuild for a burst of changes */
    rebuild->setInterval( 0 );
    connect( rebuild, &QTimer::timeout, this, &NetworkPage::rebuildLanTiles );
    settle = new QTimer( this );
    settle->setSingleShot( true );
    settle->setInterval( Library::SettleMs );
    connect( settle, &QTimer::timeout, this, &NetworkPage::updateState );
    loading = new QTimer( this );
    loading->setSingleShot( true );
    loading->setInterval( Library::NetSettleMs );
    connect( loading, &QTimer::timeout, this, &NetworkPage::updateState );

    connect( header, &LibraryHeader::backRequested, this, &NetworkPage::goBack );
    connect( list, &CardView::backRequested, this, &NetworkPage::goBack );
    connect( list, &CardView::cardActivated, this, &NetworkPage::activate );
    connect( header->search(), &QLineEdit::textChanged, this, [this]( const QString &raw ) {
        const QString text = raw.trimmed();
        if( body->currentWidget() == list )
            filter->setText( text );
        else
            layoutTiles();
        updateState();
    } );
    connect( THEMIM, &MainInputManager::leafBecameParent, this, [this]( int id ) {
        expandRequested.remove( id );   /* a node now: never expanded again */
        if( id != expandingId || !browsed )
            return;
        expandingId = -1;
        const QModelIndex source = browsed->indexByPLID( id, 0 );
        if( source.isValid() )
            browse( source );
    } );
}

/* A group title, a hint under it and its tile grid */
QLabel *NetworkPage::addGroup( const QString &title, QGridLayout **grid )
{
    const Theme *t = Theme::instance();
    QWidget *content = tilesPage->widget();
    QVBoxLayout *group = new QVBoxLayout;
    group->setSpacing( t->px( Library::GroupTitleBelow ) );
    QLabel *label = new QLabel( title, content );
    label->setFont( fontAt( Theme::PanelTitle, Library::GroupTitle, true ) );
    group->addWidget( label );
    QLabel *hint = new QLabel( content );
    hint->setProperty( "role", "hint" );
    group->addWidget( hint );
    *grid = new QGridLayout;
    ( *grid )->setSpacing( t->px( Library::TileGap ) );
    group->addLayout( *grid );
    static_cast<QVBoxLayout *>( content->layout() )->addLayout( group );
    return hint;
}

void NetworkPage::showEvent( QShowEvent *e )
{
    if( !loaded )
    {
        loaded = true;
        load();
    }
    QWidget::showEvent( e );
}

void NetworkPage::resizeEvent( QResizeEvent *e )
{
    QWidget::resizeEvent( e );
    layoutTiles();
}

void NetworkPage::load()
{
    QWidget *content = tilesPage->widget();
    NetworkTile *stream = new NetworkTile( RedesignIcons::IconOpenNetwork, qtr( "Open network stream" ),
                                           QString( "http, rtsp, rtp, smb..." ), content );
    connect( stream, &QAbstractButton::clicked, []() { THEDP->openNetDialog(); } );
    streamTiles.append( stream );

    foreach( const LibraryData::Sd &sd, LibraryData::discoveries( THEPL ) )
    {
        if( sd.category == SD_CAT_LAN )
        {
            /* P4-6: like the stock selector, only once the user asks */
            playlist_item_t *node = LibraryData::loadNode( THEPL, sd );
            if( !node )
                continue;
            const Lan lan = { new PLModel( THEPL, p_intf, node, this ),
                              sd.name.startsWith( "upnp" ) ? RedesignIcons::IconCast
                              : sd.name.startsWith( "microdns" ) ? RedesignIcons::IconServer
                              : RedesignIcons::IconStream };
            lans.append( lan );
            /* servers and shares are the rows under the discovery's node */
            const auto topLevel = [this]( const QModelIndex &parent ) {
                if( !parent.isValid() )
                    rebuild->start();
                updateState();   /* the opened server may have gone */
            };
            connect( lan.model, &QAbstractItemModel::rowsInserted, this, topLevel );
            /* queued: connected before the filter, which must have dropped
             * the rows before updateState() can reset it (showTiles) */
            connect( lan.model, &QAbstractItemModel::rowsRemoved, this, topLevel,
                     Qt::QueuedConnection );
            connect( lan.model, &QAbstractItemModel::modelReset, rebuild, QOverload<>::of( &QTimer::start ) );
            connect( lan.model, &QAbstractItemModel::dataChanged, this, [this]( const QModelIndex &i ) {
                if( !i.parent().isValid() )
                    rebuild->start();
            } );
        }
        else if( sd.name == QLatin1String( "lua{sd='icecast'}" ) || sd.name == QLatin1String( "podcast" ) )
        {
            const bool icecast = sd.name != QLatin1String( "podcast" );
            const QString name = icecast ? qtr( "Icecast directory" ) : qtr( "Podcasts" );
            NetworkTile *tile = new NetworkTile( icecast ? RedesignIcons::IconRadio : RedesignIcons::IconPodcast,
                                                 name, icecast ? qtr( "Internet radio" ) : qtr( "Subscriptions" ),
                                                 content );
            connect( tile, &QAbstractButton::clicked, this, [this, sd, name]() { openSd( sd, name ); } );
            /* Open network stream, Icecast, Podcasts, whatever order the
             * modules come in */
            streamTiles.insert( icecast ? 1 : streamTiles.count(), tile );
        }
    }
    for( int i = 1; i < streamTiles.count(); i++ )
        setTabOrder( streamTiles.at( i - 1 ), streamTiles.at( i ) );
    settle->start();
    rebuildLanTiles();
}

void NetworkPage::rebuildLanTiles()
{
    /* the keyboard stays where it was: on the tile now at that index */
    const int focused = lanTiles.indexOf( qobject_cast<NetworkTile *>( QApplication::focusWidget() ) );
    qDeleteAll( lanTiles );
    lanTiles.clear();
    foreach( const Lan &lan, lans )
        for( int i = 0; i < lan.model->rowCount(); i++ )
        {
            const QPersistentModelIndex source = lan.model->index( i, 0, QModelIndex() );
            const QString name = lan.model->getTitle( source );
            NetworkTile *tile = new NetworkTile( lan.icon, name, lan.model->getURI( source ),
                                                 tilesPage->widget() );
            PLModel *model = lan.model;
            connect( tile, &QAbstractButton::clicked, this, [this, model, source, name]() {
                if( source.isValid() )
                    open( model, source, name );
            } );
            lanTiles.append( tile );
        }
    layoutTiles();
    updateState();
    if( focused >= 0 )
    {
        if( !lanTiles.isEmpty() )
            lanTiles.at( qMin( focused, lanTiles.count() - 1 ) )->setFocus( Qt::OtherFocusReason );
        else
            tilesPage->setFocus( Qt::OtherFocusReason );
    }
}

void NetworkPage::layoutTiles()
{
    if( !loaded )
        return;
    const int cols = tileColumns[BreakpointWatcher::fromWidth( window()->width() )];
    const QString search = header->search()->text().trimmed();
    const auto place = [cols, &search]( QGridLayout *grid, const QList<NetworkTile *> &tiles ) {
        foreach( NetworkTile *tile, tiles )
            grid->removeWidget( tile );
        int n = 0;
        foreach( NetworkTile *tile, tiles )
        {
            const bool shown = matches( tile, search );
            tile->setVisible( shown );
            if( shown )
            {
                grid->addWidget( tile, n / cols, n % cols );
                n++;
            }
        }
        for( int c = 0; c < Library::NetColsL || c < grid->columnCount(); c++ )
            grid->setColumnStretch( c, c < cols ? 1 : 0 );
    };
    place( lanGrid, lanTiles );
    place( streamGrid, streamTiles );
}

void NetworkPage::openSd( const LibraryData::Sd &sd, const QString &title )
{
    PLModel *model = internet.value( sd.name );
    if( !model )
    {
        playlist_item_t *node = LibraryData::loadNode( THEPL, sd );
        if( !node )
            return;
        model = new PLModel( THEPL, p_intf, node, this );
        internet.insert( sd.name, model );
        connect( model, &QAbstractItemModel::rowsInserted, this, &NetworkPage::updateState );
        /* queued: see load() */
        connect( model, &QAbstractItemModel::rowsRemoved, this, &NetworkPage::updateState,
                 Qt::QueuedConnection );
    }
    open( model, QModelIndex(), title );
}

void NetworkPage::open( PLModel *model, const QModelIndex &source, const QString &title )
{
    loading->start();   /* "Loading..." until items come or it settles */
    header->search()->clear();
    if( browsed != model )
        filter->setSourceModel( model );
    browsed = model;
    top = source;
    topTitle = title;
    topIsItem = source.isValid();
    body->setCurrentWidget( list );
    browse( source );
    list->setFocus();
    /* a server or share not listed yet */
    if( topIsItem && source.data( VLCModelSubInterface::LEAF_NODE_ROLE ).toBool() )
        LibraryData::expand( p_intf, model, source, &expandingId, &expandRequested );
}

void NetworkPage::activate( const QModelIndex &index )
{
    if( !browsed )
        return;
    const QModelIndex source = filter->mapToSource( index );
    if( !source.data( VLCModelSubInterface::LEAF_NODE_ROLE ).toBool() )
    {
        browse( source );
        return;
    }
    if( LibraryData::isFolder( browsed, source ) )
    {
        LibraryData::expand( p_intf, browsed, source, &expandingId, &expandRequested );
        return;
    }
    input_item_t *input = browsed->getInputItem( source );
    if( !input )
        return;
    input_item_Hold( input );
    browsed->activateItem( source );
    LibraryData::revealWhenPlaying( p_intf, input, false );
}

void NetworkPage::browse( const QModelIndex &source )
{
    header->search()->clear();
    filter->setBrowseRoot( source );
    const QModelIndex root = filter->mapFromSource( source );
    list->setRootIndex( root );
    list->setCurrentIndex( filter->index( 0, 0, root ) );
    list->scrollToTop();
    updateState();
}

void NetworkPage::goBack()
{
    if( body->currentWidget() != list )
        return;
    const QModelIndex root = list->rootIndex();
    const QPersistentModelIndex from = filter->mapToSource( root );
    if( top == from )
    {
        showTiles();
        return;
    }
    header->search()->clear();
    filter->setBrowseRoot( from.parent() );
    list->setRootIndex( filter->mapFromSource( from.parent() ) );
    list->setCurrentIndex( filter->mapFromSource( from ) );
    list->scrollTo( list->currentIndex() );
    updateState();
}

void NetworkPage::showTiles()
{
    body->setCurrentWidget( tilesPage );
    header->search()->clear();
    filter->setText( QString() );
    topIsItem = false;
    tilesPage->setFocus();
    updateState();
}

void NetworkPage::updateState()
{
    const bool listing = body->currentWidget() == list;
    /* the opened server or share went away */
    if( listing && topIsItem && !top.isValid() )
    {
        showTiles();
        return;
    }
    const QModelIndex root = list->rootIndex();
    header->setFolder( !listing ? QString()
                       : top == filter->mapToSource( root ) ? topTitle : root.data().toString() );
    list->setBackEnabled( listing );

    lanHint->setVisible( lanTiles.isEmpty() );
    lanHint->setText( settle->isActive() ? qtr( "Looking for media servers and shares..." )
                                         : qtr( "No media server or share found on your network" ) );
    /* an empty list: maybe still coming (a discovery's download, a server
     * being listed; VLC 3 says neither when it is done), else nothing there
     * or nothing matches */
    QString status;
    if( listing && filter->rowCount( root ) == 0 )
        status = loading->isActive() ? qtr( "Loading..." ) : qtr( "No results" );
    header->setStatus( status );
}
