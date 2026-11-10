/*****************************************************************************
 * media_page.cpp : the Videos and Music library pages
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

#include "components/library/media_page.hpp"
#include "components/library/library_widgets.hpp"
#include "components/playlist/playlist_model.hpp"
#include "input_manager.hpp"
#include "util/theme.hpp"

#include <QItemSelectionModel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

using namespace RedesignTokens;

/* columns per breakpoint XS, S, M, L */
static const int videoColumns[] = { Library::VideoColsXS, Library::VideoColsS,
                                    Library::VideoColsM, Library::VideoColsL };
static const int albumColumns[] = { Library::AlbumColsXS, Library::AlbumColsS,
                                    Library::AlbumColsM, Library::AlbumColsL };

MediaPage::MediaPage( intf_thread_t *_p_intf, Kind _kind, QWidget *parent )
    : QWidget( parent ), p_intf( _p_intf ), kind( _kind ), model( NULL ),
      expandingId( -1 ), loaded( false ), burstRows( 0 )
{
    const Theme *t = Theme::instance();
    const QString title = kind == Videos ? qtr( "Videos" ) : qtr( "Music" );
    setAccessibleName( title );
    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setContentsMargins( t->px( Size::PagePadX ), t->px( Size::PagePadTop ),
                                t->px( Size::PagePadX ), t->px( Library::PagePadBottom ) );
    layout->setSpacing( t->px( Library::HeaderBelow ) );

    header = new LibraryHeader( title, this );
    layout->addWidget( header );

    body = new QStackedWidget( this );
    filter = new LibraryFilter( this );
    CardDelegate *delegate = new CardDelegate( kind == Videos ? CardDelegate::VideoCard
                                                              : CardDelegate::AlbumCard, filter, this );
    view = new CardView( delegate, kind == Videos ? videoColumns : albumColumns, body );
    view->setAccessibleName( title );
    view->setModel( filter );
    body->addWidget( view );
    empty = new LibraryEmptyState( body );
    body->addWidget( empty );
    layout->addWidget( body, 1 );

    settle = new QTimer( this );
    settle->setSingleShot( true );
    settle->setInterval( Library::SettleMs );
    connect( settle, &QTimer::timeout, this, [this]() {
        setAttached( true );
        updateState();
    } );

    connect( header, &LibraryHeader::backRequested, this, &MediaPage::goBack );
    connect( view, &CardView::backRequested, this, &MediaPage::goBack );
    connect( view, &CardView::cardActivated, this, &MediaPage::activate );
    connect( header->search(), &QLineEdit::textChanged, this, [this]( const QString &text ) {
        filter->setText( text );
        updateState();
    } );
}

void MediaPage::showEvent( QShowEvent *e )
{
    if( !loaded )
    {
        loaded = true;
        load();
    }
    QWidget::showEvent( e );
}

void MediaPage::load()
{
    const QLatin1String name( kind == Videos ? "video_dir" : "audio_dir" );
    playlist_item_t *node = NULL;
    foreach( const LibraryData::Sd &sd, LibraryData::discoveries( THEPL ) )
        if( sd.name == name )
            node = LibraryData::loadNode( THEPL, sd );
    if( !node )
    {
        msg_Warn( p_intf, "library: cannot load the %s discovery", name.latin1() );
        updateState();
        return;
    }

    model = new PLModel( THEPL, p_intf, node, this );
    filter->setSourceModel( model );
    connect( model, &QAbstractItemModel::rowsInserted, this,
             [this]( const QModelIndex &, int first, int last ) {
        /* QListView takes about 1 ms per inserted row (layout, accessibility):
         * a 10,000-file folder arrives as one burst of single rows and froze
         * the window for 10 s. During a burst the grid waits, detached, and
         * lays out once when the scan settles. */
        enum { BurstRows = 200, BurstWindowMs = 250 };
        if( !burstClock.isValid() || burstClock.elapsed() > BurstWindowMs )
        {
            burstClock.start();
            burstRows = 0;
        }
        burstRows += last - first + 1;
        if( burstRows > BurstRows && view->model() )
            setAttached( false );
        settle->start();   /* "Scanning..." while items keep coming */
        updateState();
    } );
    connect( model, &QAbstractItemModel::rowsRemoved, this, &MediaPage::updateState );
    connect( model, &QAbstractItemModel::modelReset, this, &MediaPage::updateState );
    /* a folder expanded by activate(): open it, as StandardPLPanel does */
    connect( THEMIM, &MainInputManager::leafBecameParent, this, [this]( int id ) {
        if( id != expandingId )
            return;
        expandingId = -1;
        const QModelIndex source = model->indexByPLID( id, 0 );
        if( source.isValid() )
            browse( source );
    } );
    settle->start();
    updateState();
}

void MediaPage::activate( const QModelIndex &index )
{
    if( !model )
        return;
    const QModelIndex source = filter->mapToSource( index );
    if( kind == Music )
    {
        input_item_t *input = LibraryData::playInPlaylist( p_intf, model, source );
        if( input )
            LibraryData::revealWhenPlaying( p_intf, input, false );
        return;
    }
    if( !source.data( VLCModelSubInterface::LEAF_NODE_ROLE ).toBool() )
    {
        browse( source );
        return;
    }
    if( LibraryData::isFolder( model, source ) )
    {
        LibraryData::expand( p_intf, model, source, &expandingId );
        return;
    }
    input_item_t *input = model->getInputItem( source );
    if( !input )
        return;
    input_item_Hold( input );
    model->activateItem( source );   /* from the discovery's node, as the stock playlist */
    LibraryData::revealWhenPlaying( p_intf, input, true );
}

void MediaPage::setAttached( bool attach )
{
    if( attach == ( view->model() != NULL ) )
        return;
    if( !attach )
        detachedRoot = filter->mapToSource( view->rootIndex() );
    QItemSelectionModel *old = view->selectionModel();
    view->setModel( attach ? filter : NULL );
    delete old;   /* setModel() leaves it to us */
    if( attach )
        view->setRootIndex( filter->mapFromSource( detachedRoot ) );
}

QModelIndex MediaPage::rootIndex() const
{
    return view->model() ? view->rootIndex() : filter->mapFromSource( detachedRoot );
}

void MediaPage::browse( const QModelIndex &source )
{
    setAttached( true );
    header->search()->clear();   /* a search is for the folder it was typed in */
    filter->setBrowseRoot( source );
    const QModelIndex root = filter->mapFromSource( source );
    view->setRootIndex( root );
    view->setCurrentIndex( filter->index( 0, 0, root ) );
    view->scrollToTop();
    updateState();
}

void MediaPage::goBack()
{
    setAttached( true );
    const QModelIndex root = view->rootIndex();
    if( !root.isValid() )
        return;
    const QPersistentModelIndex from = filter->mapToSource( root );
    header->search()->clear();
    filter->setBrowseRoot( from.parent() );
    view->setRootIndex( filter->mapFromSource( from.parent() ) );
    view->setCurrentIndex( filter->mapFromSource( from ) );
    view->scrollTo( view->currentIndex() );
    view->setFocus();
    updateState();
}

void MediaPage::updateState()
{
    /* the browsed folder may have gone: the view is back at the top then */
    const QModelIndex root = rootIndex();
    header->setFolder( root.isValid() ? root.data().toString() : QString() );
    view->setBackEnabled( root.isValid() );

    const bool scanning = settle->isActive();
    const bool none = !model || model->rowCount() == 0;
    body->setCurrentWidget( none && !scanning ? static_cast<QWidget *>( empty ) : view );
    QString status;
    if( scanning )
        status = qtr( "Scanning..." );
    else if( !none && filter->rowCount( root ) == 0 && !header->search()->text().trimmed().isEmpty() )
        status = qtr( "No results" );
    header->setStatus( status );
}
