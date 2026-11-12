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
#include "components/library/thumbnailer.hpp"
#include "components/playlist/playlist_model.hpp"
#include "input_manager.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <QApplication>
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
      loaded( false ), loadFailed( false ), burstRows( 0 )
{
    const Theme *t = Theme::instance();
    const QString title = kind == Videos ? qtr( "Videos" ) : qtr( "Music" );
    setAccessibleName( title );
    QVBoxLayout *layout = new QVBoxLayout( this );
    /* the grid keeps room for its scroll bar (CardView::updateGrid): that
     * room goes into the right padding, so the cards end under the search
     * field's right edge */
    const int scrollW = t->px( Size::ScrollBarW );
    layout->setContentsMargins( t->px( Size::PagePadX ), t->px( Size::PagePadTop ),
                                t->px( Size::PagePadX ) - scrollW, t->px( Library::PagePadBottom ) );
    layout->setSpacing( t->px( Library::HeaderBelow ) );

    header = new LibraryHeader( title, this );
    header->setContentsMargins( 0, 0, scrollW, 0 );
    layout->addWidget( header );

    body = new QStackedWidget( this );
    filter = new LibraryFilter( this );
    thumbs = kind == Videos ? Thumbnailer::instance( p_intf ) : NULL;
    CardDelegate *delegate = new CardDelegate( kind == Videos ? CardDelegate::VideoCard
                                                              : CardDelegate::AlbumCard, filter, this,
                                               thumbs );
    view = new CardView( delegate, kind == Videos ? videoColumns : albumColumns, body );
    view->setAccessibleName( title );
    view->setModel( filter );
    body->addWidget( view );
    empty = new LibraryEmptyState( qtr( "VLC shows the media of your Videos and Music folders here. "
                                        "Files are never moved or modified." ), QString(), body );
    empty->setContentsMargins( 0, 0, scrollW, 0 );   /* aligned with the header */
    body->addWidget( empty );
    notice = new LibraryNotice( body );
    notice->setContentsMargins( 0, 0, scrollW, 0 );
    body->addWidget( notice );
    layout->addWidget( body, 1 );
    connect( notice, &LibraryNotice::primaryClicked, this, [this]() { noticeAction( true ); } );
    connect( notice, &LibraryNotice::secondaryClicked, this, [this]() { noticeAction( false ); } );

    browser = new LibraryBrowser( p_intf, filter, view, header, this );
    connect( browser, &LibraryBrowser::aboutToBrowse, this, [this]() { setAttached( true ); } );
    connect( browser, &LibraryBrowser::changed, this, &MediaPage::updateState );

    settle = new QTimer( this );
    settle->setSingleShot( true );
    settle->setInterval( Library::SettleMs );
    connect( settle, &QTimer::timeout, this, [this]() {
        setAttached( true );
        updateState();
    } );

    connect( header, &LibraryHeader::backRequested, browser, &LibraryBrowser::goBack );
    connect( view, &CardView::backRequested, browser, &LibraryBrowser::goBack );
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

void MediaPage::hideEvent( QHideEvent *e )
{
    if( thumbs )
        thumbs->cancel();
    QWidget::hideEvent( e );
}

void MediaPage::load()
{
    const QLatin1String name( kind == Videos ? "video_dir" : "audio_dir" );
    playlist_item_t *node = NULL;
    foreach( const LibraryData::Sd &sd, LibraryData::discoveries( THEPL ) )
        if( sd.name == name )
            node = LibraryData::loadNode( THEPL, sd );
    loadFailed = !node;
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
        if( !burstClock.isValid() || burstClock.elapsed() > Library::BurstWindowMs )
        {
            burstClock.start();
            burstRows = 0;
        }
        burstRows += last - first + 1;
        if( burstRows > Library::BurstRows && view->model() )
            setAttached( false );
        settle->start();   /* loading while items keep coming */
        updateState();
    } );
    connect( model, &QAbstractItemModel::rowsRemoved, this, &MediaPage::updateState );
    connect( model, &QAbstractItemModel::modelReset, this, &MediaPage::updateState );
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
        browser->browse( source );
        return;
    }
    if( LibraryData::isFolder( model, source ) )
    {
        browser->expand( source );
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

void MediaPage::updateState()
{
    /* the browsed folder may have gone: the view is back at the top then */
    const QModelIndex root = rootIndex();
    header->setFolder( root.isValid() ? root.data().toString() : QString() );
    view->setBackEnabled( root.isValid() );

    const bool scanning = settle->isActive();
    const QString search = header->search()->text().trimmed();
    QWidget *page = notice;
    if( !model && loadFailed )
        notice->showMessage( RedesignIcons::IconError, Theme::Error,
                             qtr( "Your library can't be loaded" ),
                             kind == Videos ? qtr( "VLC could not list your Videos folder." )
                                            : qtr( "VLC could not list your Music folder." ),
                             qtr( "Try again" ), QString() );
    else if( !model || ( model->rowCount() == 0 && !scanning ) )
        page = empty;
    else if( !browser->showExpandState( notice, qtr( "VLC could not list \"%1\". It may be unreadable, "
                                                     "or on a drive or share that is gone." ), QString() ) )
    {
        if( !view->model() || ( scanning && filter->rowCount( root ) == 0 ) )
            notice->showLoading( qtr( "Loading..." ) );
        else if( filter->rowCount( root ) > 0 )
            page = view;
        else if( !search.isEmpty() )
            notice->showNoResults( qtr( "Nothing here matches \"%1\"." ).arg( search ) );
        else
            notice->showMessage( RedesignIcons::IconFolder, Theme::Dim, qtr( "This folder is empty" ),
                                 kind == Videos ? qtr( "There are no videos or folders in this folder." )
                                                : qtr( "There is no music in this folder." ),
                                 qtr( "Back" ), QString() );
    }

    /* the keyboard follows when its card or button goes away */
    QWidget *focus = QApplication::focusWidget();
    const bool moveFocus = focus && body->isAncestorOf( focus ) && body->currentWidget() != page;
    body->setCurrentWidget( page );
    if( moveFocus )
        page->setFocus();
}

void MediaPage::noticeAction( bool primary )
{
    if( !model && loadFailed )
        load();
    else
        browser->noticeAction( primary );
}
