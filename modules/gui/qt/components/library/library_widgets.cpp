/*****************************************************************************
 * library_widgets.cpp : pieces shared by the Videos, Music and Network pages
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

/* before vlc_threads.h, whose msleep macro breaks qthread.h */
#include <QThreadPool>

#include "components/library/library_widgets.hpp"
#include "components/library/thumbnailer.hpp"
#include "components/now_playing.hpp"         /* paintStripes, fontAt, codecName */
#include "components/playlist/playlist_model.hpp"
#include "components/playlist/selector.hpp"   /* PLSelector::loadSD, sdNode */
#include "dialogs_provider.hpp"
#include "menus.hpp"                    /* MENU_KEY_* */
#include "input_manager.hpp"
#include "main_interface.hpp"
#include "util/customwidgets.hpp"             /* QElidingLabel */
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_input_item.h>
#include <vlc_services_discovery.h>
#include <vlc_url.h>

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QPointer>
#include <QPushButton>
#include <QRunnable>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>

using namespace RedesignTokens;

/**********************************************************************
 * Data and playback
 **********************************************************************/
QList<LibraryData::Sd> LibraryData::discoveries( playlist_t *p_playlist )
{
    QList<Sd> list;
    char **longnames;
    int *categories;
    char **names = vlc_sd_GetNames( p_playlist, &longnames, &categories );
    if( !names )
        return list;
    for( int i = 0; names[i]; i++ )
    {
        const Sd sd = { qfu( names[i] ), qfu( longnames[i] ), categories[i] };
        list.append( sd );
        free( names[i] );
        free( longnames[i] );
    }
    free( names );
    free( longnames );
    free( categories );
    return list;
}

playlist_item_t *LibraryData::loadNode( playlist_t *p_playlist, const Sd &sd )
{
    bool loadedNow;
    if( !PLSelector::loadSD( p_playlist, qtu( sd.name ), &loadedNow ) )
        return NULL;
    vlc_playlist_locker lock( p_playlist );
    return PLSelector::sdNode( p_playlist, qtu( sd.longname ) );
}

bool LibraryData::isFolder( const PLModel *model, const QModelIndex &source )
{
    if( !source.data( VLCModelSubInterface::LEAF_NODE_ROLE ).toBool() )
        return true;
    input_item_t *input = model->getInputItem( source );
    return input && ( input->i_type == ITEM_TYPE_DIRECTORY || input->i_type == ITEM_TYPE_NODE );
}

void LibraryData::expand( intf_thread_t *p_intf, PLModel *model, const QModelIndex &source,
                          int *browseId, QHash<int, QElapsedTimer> *requested )
{
    input_item_t *input = model->getInputItem( source );
    const int id = model->itemId( source );
    if( !input )
        return;
    *browseId = id;   /* the latest pick opens once listed */
    /* a preparse ends within the timeout: one older is over */
    if( requested->contains( id ) && !requested->value( id ).hasExpired( Library::ExpandTimeoutMs ) )
        return;
    ( *requested )[id].start();
    /* The scan keeps subfolders collapsed (preparse depth 0); a request
     * lists one level. A network folder is the user's own pick here, as
     * when the stock playlist plays it to list it. */
    libvlc_MetadataRequest( p_intf->obj.libvlc, input,
                            input->b_net ? META_REQUEST_OPTION_SCOPE_NETWORK
                                         : META_REQUEST_OPTION_NONE, -1, NULL );
}

input_item_t *LibraryData::playInPlaylist( intf_thread_t *p_intf, PLModel *model,
                                       const QModelIndex &source )
{
    vlc_playlist_locker lock( THEPL );
    playlist_item_t *item = playlist_ItemGetById( THEPL, model->itemId( source ) );
    playlist_item_t *playing = THEPL->p_playing;
    if( !item )
        return NULL;
    const int first = playing->i_children;
    /* flattened into the playlist unless it is shown as a tree */
    if( playlist_NodeAddCopy( THEPL, item, playing, PLAYLIST_END ) <= first
     || playing->i_children <= first )
        return NULL;
    playlist_item_t *added = playing->pp_children[first];
    if( added->i_children >= 0 )
        playlist_ViewPlay( THEPL, added, NULL );   /* a folder kept as a node */
    else
        playlist_ViewPlay( THEPL, playing, added );
    input_item_Hold( added->p_input );
    return added->p_input;
}

namespace {
/* Leaves the library for the player once the item picked there plays; gives
 * up when something else plays first. One at a time: a newer pick replaces it. */
class Revealer : public QObject
{
public:
    Revealer( intf_thread_t *_p_intf, input_item_t *_item, bool video )
        : QObject( THEMIM ), p_intf( _p_intf ), item( _item )
    {
        InputManager *im = THEMIM->getIM();
        connect( im, &InputManager::playingStatusChanged, this, [this, im, video]( int state ) {
            if( state != PLAYING_S )
                return;
            if( THEMIM->currentInputItem() != item )
                deleteLater();
            else if( !video || im->hasVideo() )
                reveal();
            /* else the video output is still coming: voutChanged */
        } );
        connect( im, &InputManager::voutChanged, this, [this, im]( bool has ) {
            if( has && THEMIM->currentInputItem() == item )
                reveal();
        } );
    }
    ~Revealer() { input_item_Release( item ); }

private:
    void reveal()
    {
        /* a protected slot of the main interface */
        if( p_intf->p_sys->p_mi )
            QMetaObject::invokeMethod( p_intf->p_sys->p_mi, "showNowPlaying" );
        deleteLater();
    }

    intf_thread_t *p_intf;
    input_item_t *item;
};
}

void LibraryData::revealWhenPlaying( intf_thread_t *p_intf, input_item_t *item, bool video )
{
    static QPointer<Revealer> pending;
    delete pending;
    pending = new Revealer( p_intf, item, video );
}

/**********************************************************************
 * LibraryFilter
 **********************************************************************/
LibraryFilter::LibraryFilter( QObject *parent ) : QSortFilterProxyModel( parent )
{
}

void LibraryFilter::setBrowseRoot( const QModelIndex &source )
{
    root = source;
    if( !text.isEmpty() )
        invalidateFilter();
}

void LibraryFilter::setText( const QString &t )
{
    text = t.trimmed();
    invalidateFilter();
}

input_item_t *LibraryFilter::inputItem( const QModelIndex &proxy ) const
{
    const PLModel *model = static_cast<PLModel *>( sourceModel() );
    return model && proxy.isValid() ? model->getInputItem( mapToSource( proxy ) ) : NULL;
}

static QString takeString( char *psz )
{
    const QString s = qfu( psz );
    free( psz );
    return s;
}

QVariant LibraryFilter::data( const QModelIndex &index, int role ) const
{
    if( role == Qt::DisplayRole || role == Qt::AccessibleTextRole || role == Qt::ToolTipRole )
    {
        input_item_t *input = inputItem( index );
        return input ? takeString( input_item_GetTitleFbName( input ) ) : QString();
    }
    return QSortFilterProxyModel::data( index, role );
}

bool LibraryFilter::filterAcceptsRow( int row, const QModelIndex &sourceParent ) const
{
    /* only the browsed level: a parent filtered out would hide it */
    if( text.isEmpty() || sourceParent != root )
        return true;
    const PLModel *model = static_cast<PLModel *>( sourceModel() );
    input_item_t *input = model->getInputItem( model->index( row, 0, sourceParent ) );
    if( !input )
        return false;
    return takeString( input_item_GetTitleFbName( input ) ).contains( text, Qt::CaseInsensitive )
        || takeString( input_item_GetArtist( input ) ).contains( text, Qt::CaseInsensitive )
        || takeString( input_item_GetAlbum( input ) ).contains( text, Qt::CaseInsensitive );
}

QThreadPool *LibraryData::workerPool()
{
    /* shortcut: never deleted, so nothing waits for a read stuck on an
     * offline share (QCoreApplication waits for the global pool on exit,
     * a QThreadPool's destructor for its own); its threads end with the
     * process. Give the reads a cancel path if the pages ever get destroyed
     * and recreated while VLC runs. */
    static QThreadPool *pool = new QThreadPool;
    return pool;
}

QThreadPool *LibraryData::filePool()
{
    /* shortcut: never deleted, as workerPool() */
    static QThreadPool *pool = []() {
        QThreadPool *p = new QThreadPool;
        p->setMaxThreadCount( Library::FileWorkers );
        return p;
    }();
    return pool;
}

/**********************************************************************
 * ArtLoader
 **********************************************************************/
void ArtJob::run()
{
    QImageReader reader( path );
    reader.setDecideFormatFromContent( true );
    const QSize full = reader.size();
    if( full.isValid() )   /* decode at the size it is drawn, aspect kept */
        reader.setScaledSize( full.scaled( size, Qt::KeepAspectRatioByExpanding ) );
    emit loaded( key, reader.read() );
}

ArtLoader::ArtLoader( QObject *parent ) : QObject( parent )
{
    if( QPixmapCache::cacheLimit() < Library::ArtCacheKB )
        QPixmapCache::setCacheLimit( Library::ArtCacheKB );
}

QPixmap ArtLoader::get( const QString &path, const QSize &size )
{
    QPixmap pixmap;
    if( path.isEmpty() )
        return pixmap;
    /* a step up from the size asked, so a resize does not decode it again
     * at each pixel: paintCardThumb scales it down to the card */
    const int step = Library::ArtSizeStep;
    const QSize at( ( size.width() + step - 1 ) / step * step,
                    ( size.height() + step - 1 ) / step * step );
    const QString key = QString( "library:%1:%2x%3" ).arg( path ).arg( at.width() ).arg( at.height() );
    if( failed.contains( key ) || QPixmapCache::find( key, &pixmap ) || pending.contains( key ) )
        return pixmap;
    pending.insert( key );
    ArtJob *job = new ArtJob( path, at, key );
    /* queued: a result that comes after this loader is gone is dropped */
    connect( job, &ArtJob::loaded, this, &ArtLoader::loaded, Qt::QueuedConnection );
    LibraryData::workerPool()->start( job );
    return pixmap;
}

void ArtLoader::loaded( const QString &key, const QImage &image )
{
    pending.remove( key );
    if( image.isNull() )
    {
        if( failed.count() >= Library::ArtFailedMax )
            failed.clear();
        failed.insert( key );
        return;
    }
    QPixmapCache::insert( key, QPixmap::fromImage( image ) );
    emit ready();
}

/**********************************************************************
 * CardDelegate
 **********************************************************************/
CardDelegate::CardDelegate( Kind _kind, LibraryFilter *_filter, QObject *parent,
                            Thumbnailer *_thumbs )
    : QStyledItemDelegate( parent ), kind( _kind ), filter( _filter ),
      art( new ArtLoader( this ) ), thumbs( _thumbs ), cols( 1 ), cellW( 0 )
{
    connect( art, &ArtLoader::ready, this, &CardDelegate::repaint );
    if( thumbs )
        connect( thumbs, &Thumbnailer::changed, this, &CardDelegate::repaint );
}

void CardDelegate::setColumns( int _cols, int _cellW )
{
    cols = _cols;
    cellW = _cellW;
}

static int cardWidth( int cols, int cellW )
{
    /* the gaps are spread over the cells so the last card ends flush */
    return cellW - Theme::instance()->px( Library::GridGapX ) * ( cols - 1 ) / cols;
}

int CardDelegate::cardHeight( int cardW ) const
{
    const Theme *t = Theme::instance();
    const int thumbH = kind == AlbumCard ? cardW
                     : cardW * Library::ThumbAspectH / Library::ThumbAspectW;
    return thumbH + t->px( Library::CardTextGap )
         + QFontMetrics( fontAt( Theme::Body, Library::CardTitle, true ) ).height()
         + t->px( Library::CardMetaGap ) + QFontMetrics( t->font( Theme::Meta ) ).height();
}

QSize CardDelegate::sizeHint( const QStyleOptionViewItem &, const QModelIndex & ) const
{
    if( kind == Row )
        return QSize( 0, Theme::instance()->metric( Theme::PlaylistRowH ) );
    return QSize( cellW, cardHeight( cardWidth( cols, cellW ) ) );
}

void CardDelegate::paint( QPainter *p, const QStyleOptionViewItem &option,
                          const QModelIndex &index ) const
{
    p->save();
    p->setRenderHint( QPainter::Antialiasing );
    if( kind == Row )
        paintRow( p, option, index );
    else
        paintCard( p, option, index );
    p->restore();
}

/* An icon at a device-independent size, tinted, centred in r */
static void paintGlyph( QPainter *p, const char *name, Theme::ColorRole role,
                        int sizePx, const QRectF &r )
{
    Theme *t = Theme::instance();
    const int devicePx = qRound( sizePx * p->device()->devicePixelRatioF() );
    const QRect box( qRound( r.center().x() - sizePx / 2. ), qRound( r.center().y() - sizePx / 2. ),
                     sizePx, sizePx );
    t->icon( QLatin1String( name ), role, QSize( devicePx, devicePx ) ).paint( p, box );
}

void CardDelegate::paintCard( QPainter *p, const QStyleOptionViewItem &option,
                              const QModelIndex &index ) const
{
    Theme *t = Theme::instance();
    input_item_t *input = filter->inputItem( index );
    if( !input || cols <= 0 || cellW <= 0 )
        return;
    const PLModel *model = static_cast<PLModel *>( filter->sourceModel() );
    const QModelIndex source = filter->mapToSource( index );
    const bool folder = LibraryData::isFolder( model, source );

    const int col = qBound( 0, qRound( qreal( option.rect.x() ) / cellW ), cols - 1 );
    const int cardW = cardWidth( cols, cellW );
    const int x = option.rect.x() + col * t->px( Library::GridGapX ) / cols;
    const int thumbH = kind == AlbumCard ? cardW
                     : cardW * Library::ThumbAspectH / Library::ThumbAspectW;
    const QRectF thumb = QRectF( x, option.rect.y(), cardW, thumbH ).adjusted( .5, .5, -.5, -.5 );
    const qreal radius = t->px( Library::CardRadius );

    const qreal dpr = p->device()->devicePixelRatioF();
    paintCardThumb( p, thumb, radius, art->get( artPath( input, source ), ( thumb.size() * dpr ).toSize() ),
                    folder ? RedesignIcons::IconFolder
                    : kind == AlbumCard ? RedesignIcons::IconMusic : RedesignIcons::IconVideo,
                    t->px( Library::CardStripe ), t->px( Library::CardGlyph ) );
    const qreal watched = progress( input, source );
    if( watched > 0 )
        paintProgress( p, thumb, watched, option.direction );
    p->setClipping( false );

    /* DESIGN_SPEC 4: hover is a 1 px accent border */
    const bool hover = option.state & QStyle::State_MouseOver;
    p->setPen( QPen( hover ? t->color( Theme::Accent ) : t->ink( InkAlpha::Divider ), Size::Hairline ) );
    p->setBrush( Qt::NoBrush );
    p->drawRoundedRect( thumb, radius, radius );
    if( ( option.state & QStyle::State_HasFocus ) && Theme::showsFocus( option.widget ) )
        Theme::paintFocusRing( p, thumb, radius - Size::FocusRing / 2. );

    /* duration badge */
    const int64_t duration = folder ? -1 : input_item_GetDuration( input );
    if( duration > 0 )
        paintTimeBadge( p, thumb, timeString( duration / CLOCK_FREQ ), t->font( Theme::Overline ),
                        option.direction, t->px( Library::BadgeInset ), t->px( Library::BadgeInset ),
                        t->px( Library::BadgePadX ), t->px( Library::BadgePadY ) );

    /* title and meta */
    const QFont titleFont = fontAt( Theme::Body, Library::CardTitle, true );
    const QFont metaFont = t->font( Theme::Meta );
    const QFontMetrics titleFm( titleFont ), metaFm( metaFont );
    const int titleY = qRound( thumb.bottom() ) + t->px( Library::CardTextGap );
    const QRect titleRect( x, titleY, cardW, titleFm.height() );
    const QRect metaRect( x, titleRect.bottom() + 1 + t->px( Library::CardMetaGap ), cardW, metaFm.height() );
    p->setFont( titleFont );
    p->setPen( t->color( Theme::Text ) );
    p->drawText( titleRect.translated( 0, -t->textNudge( p->font() ) ), Qt::AlignLeft | Qt::AlignVCenter,
                 titleFm.elidedText( index.data().toString(), Qt::ElideRight, cardW ) );
    p->setFont( metaFont );
    p->setPen( t->color( Theme::Mute ) );
    p->drawText( metaRect.translated( 0, -t->textNudge( p->font() ) ), Qt::AlignLeft | Qt::AlignVCenter,
                 metaFm.elidedText( metaLine( input, source ), Qt::ElideRight, cardW ) );
}

void CardDelegate::paintRow( QPainter *p, const QStyleOptionViewItem &option,
                             const QModelIndex &index ) const
{
    Theme *t = Theme::instance();
    input_item_t *input = filter->inputItem( index );
    if( !input )
        return;
    const PLModel *model = static_cast<PLModel *>( filter->sourceModel() );
    const QModelIndex source = filter->mapToSource( index );
    const QRectF r = QRectF( option.rect ).adjusted( .5, .5, -.5, -.5 );
    const qreal radius = t->metric( Theme::RadiusMd );
    if( option.state & QStyle::State_MouseOver )
    {
        p->setPen( Qt::NoPen );
        p->setBrush( t->ink( InkAlpha::HoverFill ) );
        p->drawRoundedRect( r, radius, radius );
    }
    if( ( option.state & QStyle::State_HasFocus ) && Theme::showsFocus( option.widget ) )
        Theme::paintFocusRing( p, r, radius - Size::FocusRing / 2. );

    const int padX = t->px( Playlist::RowPadX ), gap = t->px( Playlist::RowGap );
    const int iconPx = t->px( Playlist::RowIcon );
    const QRect iconRect( option.rect.x() + padX, option.rect.center().y() - iconPx / 2, iconPx, iconPx );
    paintGlyph( p, LibraryData::isFolder( model, source ) ? RedesignIcons::IconFolder : RedesignIcons::IconOpenFile,
                Theme::AccentText, iconPx, QStyle::visualRect( option.direction, option.rect, iconRect ) );

    p->setFont( t->font( Theme::Body ) );
    const QFontMetrics fm( p->font() );
    const int64_t duration = input_item_GetDuration( input );
    const QString time = duration > 0 ? timeString( duration / CLOCK_FREQ ) : QString();
    const int timeW = time.isEmpty() ? 0 : fm.width( time ) + gap;
    const QRect timeRect( option.rect.right() - padX - timeW, option.rect.y(), timeW, option.rect.height() );
    const QRect titleRect( iconRect.right() + 1 + gap, option.rect.y(),
                           timeRect.left() - iconRect.right() - 1 - gap, option.rect.height() );
    p->setPen( t->color( Theme::Text ) );
    /* the alignments follow the painter's direction, the rects do not */
    const int nudge = t->textNudge( p->font() );
    p->drawText( QStyle::visualRect( option.direction, option.rect, titleRect ).translated( 0, -nudge ),
                 Qt::AlignLeft | Qt::AlignVCenter,
                 fm.elidedText( index.data().toString(), Qt::ElideRight, titleRect.width() ) );
    p->setPen( t->color( Theme::Mute ) );
    p->drawText( QStyle::visualRect( option.direction, option.rect, timeRect ).translated( 0, -nudge ),
                 Qt::AlignRight | Qt::AlignVCenter, time );
}

QString CardDelegate::artPath( input_item_t *input, const QModelIndex &source ) const
{
    QString path = InputManager::decodeArtURL( input );
    /* a folder shows the art of its first items, as PLModel's cover column;
     * shortcut: only the first few, so a big folder costs nothing per paint */
    const PLModel *model = static_cast<PLModel *>( filter->sourceModel() );
    for( int i = 0; path.isEmpty() && i < qMin<int>( Library::ArtProbe, model->rowCount( source ) ); i++ )
        path = InputManager::decodeArtURL( model->getInputItem( model->index( i, 0, source ) ) );
    if( !path.isEmpty() || !thumbs )
        return path;

    /* no art: a frame of the video, or of a folder's first video */
    const QString local = localPath( input );
    if( local.isEmpty() )
        return path;
    if( !LibraryData::isFolder( model, source ) )
        return thumbs->frame( local, input_item_GetDuration( input ) / 1000 );
    return thumbs->folderFrame( local );
}

qreal CardDelegate::progress( input_item_t *input, const QModelIndex &source ) const
{
    const QString local = thumbs ? localPath( input ) : QString();
    if( local.isEmpty() )
        return -1;
    const PLModel *model = static_cast<PLModel *>( filter->sourceModel() );
    if( !LibraryData::isFolder( model, source ) )
        return thumbs->resume( local );
    /* a folder: how many of its videos were watched */
    const QStringList *videos = thumbs->videos( local );
    if( !videos || videos->isEmpty() )
        return -1;
    return qreal( thumbs->watchedIn( local ) ) / videos->count();
}

QString CardDelegate::metaLine( input_item_t *input, const QModelIndex &source ) const
{
    const PLModel *model = static_cast<PLModel *>( filter->sourceModel() );
    if( LibraryData::isFolder( model, source ) )
    {
        const QString local = thumbs ? localPath( input ) : QString();
        const QStringList *videos = local.isEmpty() ? NULL : thumbs->videos( local );
        if( videos && !videos->isEmpty() )
            return QString::fromUtf8( vlc_ngettext( "%1 video", "%1 videos", videos->count() ) )
                   .arg( videos->count() );
        if( source.data( VLCModelSubInterface::LEAF_NODE_ROLE ).toBool() )
            return qtr( "Folder" );     /* not expanded yet */
        const int n = model->rowCount( source );
        return QString::fromUtf8( vlc_ngettext( "%1 item", "%1 items", n ) ).arg( n );
    }

    const QString line = kind == AlbumCard ? takeString( input_item_GetArtist( input ) )
                                           : mediaMetaLine( input );
    return line.isEmpty() ? QString::fromUtf8( "—" ) : line;
}

QString localPath( const QString &mrl )
{
    char *path = vlc_uri2path( qtu( mrl ) );
    const QString local = qfu( path );
    free( path );
    return local;
}

QString localPath( input_item_t *input )
{
    char *uri = input_item_GetURI( input );
    const QString local = uri ? localPath( qfu( uri ) ) : QString();
    free( uri );
    return local;
}

bool hasExtension( const QString &path, const QStringList &filters )
{
    return filters.contains( QLatin1String( "*." ) + QFileInfo( path ).suffix().toLower() );
}

QString metaSeparator()
{
    return QString::fromUtf8( " \xC2\xB7 " );
}

void paintCardThumb( QPainter *p, const QRectF &thumb, qreal radius, QPixmap pixmap,
                     const char *glyph, int stripePx, int glyphPx )
{
    QPainterPath clip;
    clip.addRoundedRect( thumb, radius, radius );
    p->setClipPath( clip );
    if( pixmap.isNull() )
    {
        paintStripes( *p, thumb, stripePx );
        paintGlyph( p, glyph, Theme::Dim, glyphPx, thumb );
        return;
    }
    /* cropped to fill; a little bigger than the card (ArtLoader) */
    const QSizeF s = QSizeF( pixmap.size() ).scaled( thumb.size(), Qt::KeepAspectRatioByExpanding );
    p->setRenderHint( QPainter::SmoothPixmapTransform );
    p->drawPixmap( QRectF( thumb.center().x() - s.width() / 2, thumb.center().y() - s.height() / 2,
                           s.width(), s.height() ), pixmap, QRectF( pixmap.rect() ) );
}

void paintTimeBadge( QPainter *p, const QRectF &thumb, const QString &text, const QFont &font,
                     Qt::LayoutDirection dir, int insetX, int insetY, int padX, int padY )
{
    Theme *t = Theme::instance();
    const QFontMetrics fm( font );
    const QSizeF size( fm.width( text ) + 2 * padX, fm.height() + 2 * padY );
    QRectF badge( thumb.right() - insetX - size.width(), thumb.bottom() - insetY - size.height(),
                  size.width(), size.height() );
    if( dir == Qt::RightToLeft )
        badge.moveLeft( thumb.left() + thumb.right() - badge.right() );
    const qreal radius = t->px( Library::BadgeRadius );
    p->setPen( Qt::NoPen );
    p->setBrush( QColor::fromRgba( CardBadgeBg ) );
    p->drawRoundedRect( badge, radius, radius );
    p->setFont( font );
    p->setPen( t->color( Theme::OnVideo ) );
    p->drawText( badge, Qt::AlignCenter, text );
}

void paintProgress( QPainter *p, const QRectF &thumb, qreal fraction, Qt::LayoutDirection dir )
{
    const Theme *t = Theme::instance();
    const qreal h = t->px( Home::Progress );
    const QRectF track( thumb.left(), thumb.bottom() - h, thumb.width(), h );
    p->fillRect( track, t->ink( InkAlpha::ProgressTrack ) );
    QRectF done( track.left(), track.top(), track.width() * qBound<qreal>( 0, fraction, 1 ), h );
    if( dir == Qt::RightToLeft )
        done.moveRight( track.right() );
    p->fillRect( done, t->color( Theme::Accent ) );
}

QString mediaMetaLine( input_item_t *input )
{
    QStringList parts;
    const QString date = takeString( input_item_GetDate( input ) );
    parts << ( date.left( 4 ).toInt() > 0 ? date.left( 4 ) : QString() );
    int height = 0, channels = 0;
    vlc_fourcc_t codec = 0;
    vlc_mutex_lock( &input->lock );
    for( int i = 0; i < input->i_es; i++ )
    {
        const es_format_t *es = input->es[i];
        if( es->i_cat == VIDEO_ES && codec == 0 )
        {
            codec = es->i_codec;
            height = es->video.i_visible_height ? es->video.i_visible_height : es->video.i_height;
        }
        else if( es->i_cat == AUDIO_ES && channels == 0 )
            channels = es->audio.i_channels;
    }
    vlc_mutex_unlock( &input->lock );
    if( height >= 2160 )
        parts << qtr( "4K" );
    else if( height > 0 )
        parts << QString( "%1p" ).arg( height );
    if( codec )
    {
        char fourcc[5] = { 0 };
        vlc_fourcc_to_char( codec, fourcc );
        parts << codecName( QString::fromLatin1( fourcc ) );
    }
    if( channels == 6 )
        parts << QString( "5.1" );
    else if( channels == 8 )
        parts << QString( "7.1" );
    parts.removeAll( QString() );
    return parts.join( metaSeparator() );
}

/**********************************************************************
 * CardView
 **********************************************************************/
CardView::CardView( CardDelegate *_delegate, const int *_columns, QWidget *parent )
    : QListView( parent ), delegate( _delegate ), columns( _columns ), backEnabled( false )
{
    setProperty( "role", "library" );
    setItemDelegate( delegate );
    setFrameShape( QFrame::NoFrame );
    setSelectionMode( QAbstractItemView::NoSelection );
    setEditTriggers( QAbstractItemView::NoEditTriggers );
    setUniformItemSizes( true );
    setMouseTracking( true );
    viewport()->setAttribute( Qt::WA_Hover );
    setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    /* lays out a 10,000-item folder in steps instead of in one go */
    setLayoutMode( QListView::Batched );
    if( columns )
    {
        setFlow( QListView::LeftToRight );
        setWrapping( true );
        setResizeMode( QListView::Adjust );
    }
    connect( delegate, &CardDelegate::repaint, viewport(), QOverload<>::of( &QWidget::update ) );
}

void CardView::updateGrid()
{
    if( !columns )
        return;
    const int cols = columns[BreakpointWatcher::fromWidth( window()->width() )];
    /* the scroll bar's room is kept, so it cannot change the column width */
    const int w = contentsRect().width() - verticalScrollBar()->sizeHint().width();
    /* QListView wraps a row that fills the viewport exactly: 1 px to spare */
    const int cellW = qMax( 1, ( w - 1 ) / cols );
    delegate->setColumns( cols, cellW );
    const QSize grid( cellW, delegate->cardHeight( cardWidth( cols, cellW ) )
                             + Theme::instance()->px( Library::GridGapY ) );
    if( grid != gridSize() )
        setGridSize( grid );
}

void CardView::resizeEvent( QResizeEvent *e )
{
    updateGrid();
    QListView::resizeEvent( e );
}

void CardView::mousePressEvent( QMouseEvent *e )
{
    QListView::mousePressEvent( e );
    pressed = e->button() == Qt::LeftButton ? indexAt( e->pos() ) : QModelIndex();
}

void CardView::mouseReleaseEvent( QMouseEvent *e )
{
    QListView::mouseReleaseEvent( e );
    const QModelIndex index = indexAt( e->pos() );
    const bool click = e->button() == Qt::LeftButton && index.isValid() && index == pressed;
    pressed = QModelIndex();
    if( click )
        emit cardActivated( index );
}

/* One click opens a card: the second click of a double click does nothing */
void CardView::mouseDoubleClickEvent( QMouseEvent *e )
{
    e->accept();
}

void CardView::keyPressEvent( QKeyEvent *e )
{
    switch( e->key() )
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if( currentIndex().isValid() )
            emit cardActivated( currentIndex() );
        return;
    case Qt::Key_Backspace:
    case Qt::Key_Escape:
        if( backEnabled )
        {
            emit backRequested();
            return;
        }
        break;
    default:
        break;
    }
    QListView::keyPressEvent( e );
}

/**********************************************************************
 * LibraryBrowser
 **********************************************************************/
LibraryBrowser::LibraryBrowser( intf_thread_t *_p_intf, LibraryFilter *_filter, CardView *_view,
                                LibraryHeader *_header, QObject *parent )
    : QObject( parent ), p_intf( _p_intf ), filter( _filter ), view( _view ), header( _header ),
      expandingId( -1 ), expandFailed( false )
{
    expandTimer = new QTimer( this );
    expandTimer->setSingleShot( true );
    expandTimer->setInterval( Library::ExpandTimeoutMs );
    connect( expandTimer, &QTimer::timeout, this, [this]() {
        expandFailed = true;   /* still browsed into if its items come later */
        emit changed();
    } );
    /* a folder expanded by expand(): open it, as StandardPLPanel does */
    connect( THEMIM, &MainInputManager::leafBecameParent, this, [this]( int id ) {
        expandRequested.remove( id );   /* a node now: never expanded again */
        if( id != expandingId || !model() )
            return;
        expandingId = -1;
        expandTimer->stop();
        expandFailed = false;
        const QModelIndex source = model()->indexByPLID( id, 0 );
        if( source.isValid() )
            browse( source );
    } );
}

PLModel *LibraryBrowser::model() const
{
    return static_cast<PLModel *>( filter->sourceModel() );
}

bool LibraryBrowser::expanding() const
{
    return expandTimer->isActive();
}

void LibraryBrowser::browse( const QModelIndex &source )
{
    emit aboutToBrowse();
    expandTimer->stop();
    expandFailed = false;
    header->search()->clear();   /* a search is for the folder it was typed in */
    filter->setBrowseRoot( source );
    const QModelIndex root = filter->mapFromSource( source );
    view->setRootIndex( root );
    view->setCurrentIndex( filter->index( 0, 0, root ) );
    view->scrollToTop();
    emit changed();
}

void LibraryBrowser::expand( const QModelIndex &source )
{
    expandingIndex = source;
    expandFailed = false;
    expandTimer->start();   /* the loading state until it is listed */
    LibraryData::expand( p_intf, model(), source, &expandingId, &expandRequested );
    emit changed();
}

void LibraryBrowser::cancelExpand()
{
    /* its request may still run: expand() asks again once that is over */
    expandTimer->stop();
    expandFailed = false;
    expandingId = -1;
}

void LibraryBrowser::goBack()
{
    emit aboutToBrowse();
    if( ( expandTimer->isActive() || expandFailed )
        && ( !expandingIndex.isValid() || expandingIndex != top ) )
    {
        /* gives up on the folder being opened: back to its parent's cards */
        cancelExpand();
        emit changed();
        view->setFocus();
        return;
    }
    const QPersistentModelIndex from = filter->mapToSource( view->rootIndex() );
    if( from == top )
    {
        emit backFromTop();
        return;
    }
    header->search()->clear();
    filter->setBrowseRoot( from.parent() );
    view->setRootIndex( filter->mapFromSource( from.parent() ) );
    view->setCurrentIndex( filter->mapFromSource( from ) );
    view->scrollTo( view->currentIndex() );
    view->setFocus();
    emit changed();
}

void LibraryBrowser::noticeAction( bool primary )
{
    if( expandFailed && primary && expandingIndex.isValid() )
        expand( expandingIndex );   /* the timeout let it be asked again */
    else if( !expandFailed && !header->search()->text().trimmed().isEmpty() )
    {
        header->search()->clear();
        view->setFocus();
    }
    else
        goBack();
}

bool LibraryBrowser::showExpandState( LibraryNotice *notice, const QString &cannotList,
                                      const QString &name ) const
{
    if( expandTimer->isActive() )
        notice->showLoading( qtr( "Loading..." ) );
    else if( expandFailed )
        notice->showMessage( RedesignIcons::IconError, Theme::Error, qtr( "This folder can't be opened" ),
                             cannotList.arg( expandingIndex.isValid() ? model()->getTitle( expandingIndex ) : name ),
                             qtr( "Try again" ), qtr( "Back" ) );
    else
        return false;
    return true;
}

/**********************************************************************
 * LibraryHeader
 **********************************************************************/
LibraryHeader::LibraryHeader( const QString &_title, QWidget *parent )
    : QWidget( parent ), pageTitle( _title )
{
    const Theme *t = Theme::instance();
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( t->px( Library::HeaderGap ) );

    back = new QToolButton( this );
    back->setIcon( backIcon( Theme::Text ) );
    back->setIconSize( QSize( t->metric( Theme::IconGlyph ), t->metric( Theme::IconGlyph ) ) );
    back->setFixedSize( t->metric( Theme::IconButton ), t->metric( Theme::IconButton ) );
    back->setAccessibleName( qtr( "Back" ) );
    back->setToolTip( withHotkey( NULL, qtr( "Back" ), "Backspace" ) );
    back->setFocusPolicy( Qt::TabFocus );
    back->hide();
    connect( back, &QToolButton::clicked, this, &LibraryHeader::backRequested );
    layout->addWidget( back );

    title = new QElidingLabel( pageTitle, Qt::ElideRight, this );
    title->setFont( t->font( Theme::PageTitle ) );
    title->setAccessibleName( pageTitle );
    layout->addWidget( title, 1 );

    searchEdit = new QLineEdit( this );
    searchEdit->setProperty( "role", "librarySearch" );
    searchEdit->setPlaceholderText( qtr( "Search media library" ) );
    searchEdit->setAccessibleName( qtr( "Search media library" ) );
    setupSearchField( searchEdit, qtr( "Search" ), qtr( "Clear search" ) );
    searchEdit->setFixedHeight( t->metric( Theme::InputH ) );
    layout->addWidget( searchEdit );
}

void LibraryHeader::setFolder( const QString &name )
{
    title->setText( name.isEmpty() ? pageTitle : name );
    /* called for each row of a scan: Qt 5.6 tells screen readers of each set */
    if( title->accessibleName() != title->text() )
        title->setAccessibleName( title->text() );
    back->setVisible( !name.isEmpty() );
}

void LibraryHeader::resizeEvent( QResizeEvent *e )
{
    static const int widths[] = { Library::SearchXS, Library::SearchS, Library::SearchM, Library::SearchL };
    searchEdit->setFixedWidth( Theme::instance()->px(
        widths[BreakpointWatcher::fromWidth( window()->width() )] ) );
    QWidget::resizeEvent( e );
}

/**********************************************************************
 * LibraryEmptyState
 **********************************************************************/
namespace {
/* Wrapped text sized once to its lines (below). It must not tell the
 * layouts its height depends on its width: a QScrollArea (Home) then sizes
 * the whole page by height-for-width, past a short window, with a scroll
 * bar, although the page's minimum fits. */
class FixedTextLabel : public QLabel
{
public:
    FixedTextLabel( const QString &text, QWidget *parent ) : QLabel( text, parent ) {}
    bool hasHeightForWidth() const Q_DECL_OVERRIDE { return false; }
};

/* The column of the empty state and the notices */
QVBoxLayout *noticeLayout( QWidget *w )
{
    const Theme *t = Theme::instance();
    QVBoxLayout *layout = new QVBoxLayout( w );
    const int pad = t->px( Library::EmptyPad );
    layout->setContentsMargins( pad, 0, pad, 0 );
    layout->setSpacing( t->px( Library::EmptyGap ) );
    return layout;
}

/* Its wrapped hint text, plain (a search or a server's name goes in) */
QLabel *noticeText( const QString &text, QWidget *parent )
{
    QLabel *l = new FixedTextLabel( text, parent );
    l->setTextFormat( Qt::PlainText );
    l->setProperty( "role", "hint" );
    l->setWordWrap( true );
    l->setAlignment( Qt::AlignCenter );
    /* an aligned label gets its one-line size hint: give it its wrapped size */
    l->setFixedWidth( Theme::instance()->px( Library::EmptyTextW ) );
    l->setFixedHeight( l->heightForWidth( l->width() ) );
    return l;
}

/* Its buttons, in a centred row */
void addNoticeButtons( QVBoxLayout *layout, const QList<QPushButton *> &buttons )
{
    const Theme *t = Theme::instance();
    QHBoxLayout *row = new QHBoxLayout;
    row->setSpacing( t->px( Library::EmptyButtonGap ) );
    foreach( QPushButton *b, buttons )
    {
        b->setFixedHeight( t->px( Library::EmptyButtonH ) );
        b->setFocusPolicy( Qt::TabFocus );
        b->setCursor( Qt::PointingHandCursor );
        row->addWidget( b );
    }
    layout->addSpacing( t->px( Library::EmptyButtonsTop ) );
    layout->addLayout( row );
    layout->setAlignment( row, Qt::AlignHCenter );
}
}

LibraryEmptyState::LibraryEmptyState( const QString &text, const QString &note, QWidget *parent )
    : QWidget( parent ), noteLabel( NULL )
{
    Theme *t = Theme::instance();
    QVBoxLayout *layout = noticeLayout( this );
    const int pad = t->px( Library::EmptyPad );
    /* the top and bottom padding is a centring spacer each: `pad` tall
     * when there is room, down to nothing in a short window */
    layout->addSpacerItem( new QSpacerItem( 0, pad, QSizePolicy::Minimum, QSizePolicy::Expanding ) );

    QLabel *cone = new QLabel( this );
    const int conePx = t->px( Library::EmptyCone );
    /* the original cone, as drawn, at the size nearest the screen's */
    QIcon logo;
    logo.addFile( ":/logo/vlc48.png" );
    logo.addFile( ":/logo/vlc128.png" );
    logo.addFile( ":/logo/vlc256.png" );
    cone->setPixmap( logo.pixmap( conePx, conePx ) );
    layout->addWidget( cone, 0, Qt::AlignHCenter );

    QLabel *title = new QLabel( qtr( "Your library is empty" ), this );
    title->setFont( fontAt( Theme::PanelTitle, Library::EmptyTitle, true ) );
    layout->addWidget( title, 0, Qt::AlignHCenter );

    layout->addWidget( noticeText( text, this ), 0, Qt::AlignHCenter );
    if( !note.isEmpty() )
        layout->addWidget( noteLabel = noticeText( note, this ), 0, Qt::AlignHCenter );

    /* P4-2: the folder goes to the playlist; the scanned folders are the
     * system's Videos and Music folders */
    QPushButton *add = new QPushButton( qtr( "Add a folder" ), this );
    add->setProperty( "role", "primary" );
    add->setIcon( t->icon( RedesignIcons::IconAddFolder, Theme::OnAccent ) );
    add->setToolTip( withHotkey( NULL, qtr( "Add a folder to the playlist and play it" ),
                                MENU_KEY_OPEN_FOLDER ) );
    connect( add, &QPushButton::clicked, []() { THEDP->PLOpenDir(); } );
    QPushButton *open = new QPushButton( qtr( "Open media..." ), this );
    open->setProperty( "role", "outline" );
    open->setToolTip( withHotkey( NULL, qtr( "Open media" ), MENU_KEY_OPEN_MEDIA ) );
    connect( open, &QPushButton::clicked, []() { THEDP->openDialog(); } );
    const int iconPx = t->px( Library::EmptyButtonIcon );
    add->setIconSize( QSize( iconPx, iconPx ) );
    open->setIconSize( QSize( iconPx, iconPx ) );
    addNoticeButtons( layout, QList<QPushButton *>() << add << open );

    QLabel *hint = new QLabel( qtr( "or drop files anywhere in this window" ), this );
    hint->setFont( t->font( Theme::Mono ) );
    hint->setProperty( "role", "libraryDim" );
    layout->addWidget( hint, 0, Qt::AlignHCenter );
    layout->addSpacerItem( new QSpacerItem( 0, pad, QSizePolicy::Minimum, QSizePolicy::Expanding ) );
}

void LibraryEmptyState::setNoteVisible( bool visible )
{
    if( noteLabel )
        noteLabel->setVisible( visible );
}

QSize LibraryEmptyState::sizeHint() const
{
    /* EmptyMinH when there is room; a short window shrinks it down to its
     * content (minimumSizeHint) instead of scrolling the buttons away */
    QSize s = QWidget::sizeHint();
    s.setHeight( qMax( s.height(), Theme::instance()->px( Library::EmptyMinH ) ) );
    return s;
}

void LibraryEmptyState::paintEvent( QPaintEvent * )
{
    const Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    const qreal w = Library::EmptyBorderTenths / 10.;
    QPen pen( t->ink( Library::EmptyBorderAlpha ), w, Qt::DashLine );
    p.setPen( pen );
    p.setBrush( Qt::NoBrush );
    const qreal radius = t->metric( Theme::RadiusTile );
    p.drawRoundedRect( QRectF( contentsRect() ).adjusted( w / 2, w / 2, -w / 2, -w / 2 ), radius, radius );
}

/**********************************************************************
 * LoadingRing
 **********************************************************************/
LoadingRing::LoadingRing( QWidget *parent ) : QWidget( parent )
{
    setAccessibleName( qtr( "Loading" ) );
    spin = new QVariantAnimation( this );
    spin->setStartValue( 0. );
    spin->setEndValue( 360. );
    spin->setLoopCount( -1 );
    connect( spin, &QVariantAnimation::valueChanged, this,
             static_cast<void (QWidget::*)()>( &QWidget::update ) );
}

QSize LoadingRing::sizeHint() const
{
    const int px = Theme::instance()->px( Library::NoticeRing );
    return QSize( px, px );
}

void LoadingRing::showEvent( QShowEvent *e )
{
    /* 0: Windows animations are off, a still ring */
    const int ms = Theme::instance()->duration( Theme::Spinner );
    if( ms )
    {
        spin->setDuration( ms );
        spin->start();
    }
    QWidget::showEvent( e );
}

void LoadingRing::hideEvent( QHideEvent *e )
{
    spin->stop();
    QWidget::hideEvent( e );
}

void LoadingRing::paintEvent( QPaintEvent * )
{
    const Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    const qreal w = t->px( Library::NoticeRingWidth );
    const int side = qMin( width(), height() );
    const QRectF r = QRectF( ( width() - side ) / 2., ( height() - side ) / 2., side, side )
                         .adjusted( w / 2, w / 2, -w / 2, -w / 2 );
    p.setPen( QPen( t->ink( InkAlpha::Track ), w ) );
    p.drawEllipse( r );
    /* the accent top quarter, turning clockwise (Qt's angles turn the other way) */
    const qreal turn = spin->state() == QAbstractAnimation::Running ? spin->currentValue().toReal() : 0.;
    p.setPen( QPen( t->color( Theme::Accent ), w, Qt::SolidLine, Qt::FlatCap ) );
    p.drawArc( r, qRound( ( 45. - turn ) * 16 ), 90 * 16 );
}

/**********************************************************************
 * LibraryNotice
 **********************************************************************/
LibraryNotice::LibraryNotice( QWidget *parent )
    : QWidget( parent ), glyphIcon( NULL ), glyphRole( Theme::Dim )
{
    QVBoxLayout *layout = noticeLayout( this );
    layout->addStretch( 1 );

    ring = new LoadingRing( this );
    layout->addWidget( ring, 0, Qt::AlignHCenter );
    glyph = new QLabel( this );
    layout->addWidget( glyph, 0, Qt::AlignHCenter );
    title = new QLabel( this );
    title->setTextFormat( Qt::PlainText );
    title->setFont( fontAt( Theme::PanelTitle, Library::EmptyTitle, true ) );
    layout->addWidget( title, 0, Qt::AlignHCenter );
    text = noticeText( QString(), this );
    layout->addWidget( text, 0, Qt::AlignHCenter );

    primary = new QPushButton( this );
    primary->setProperty( "role", "primary" );
    connect( primary, &QPushButton::clicked, this, &LibraryNotice::primaryClicked );
    secondary = new QPushButton( this );
    secondary->setProperty( "role", "outline" );
    connect( secondary, &QPushButton::clicked, this, &LibraryNotice::secondaryClicked );
    addNoticeButtons( layout, QList<QPushButton *>() << primary << secondary );
    layout->addStretch( 1 );
    /* a page moving the focus here from its grid lands on the action */
    setFocusProxy( primary );
    connect( Theme::instance(), &Theme::changed, this, &LibraryNotice::paintGlyph );
}

/* in the theme's colours: again when they change */
void LibraryNotice::paintGlyph()
{
    if( !glyphIcon )
        return;
    Theme *t = Theme::instance();
    const int px = t->px( Library::NoticeGlyph );
    glyph->setPixmap( t->icon( QLatin1String( glyphIcon ), glyphRole, QSize( px, px ) ).pixmap( px, px ) );
}

void LibraryNotice::setText( QLabel *label, const QString &s )
{
    /* called for each row of a scan */
    if( label->text() == s && label->isVisibleTo( this ) == !s.isEmpty() )
        return;
    label->setText( s );
    label->setVisible( !s.isEmpty() );
    if( label == text )   /* its wrapped size, as LibraryEmptyState's */
        label->setFixedHeight( label->heightForWidth( label->width() ) );
}

void LibraryNotice::showLoading( const QString &caption )
{
    glyph->hide();
    title->hide();
    setText( text, caption );
    primary->hide();
    secondary->hide();
    ring->show();
    /* called for each row of a scan: Qt 5.6 tells screen readers of each set */
    if( accessibleName() != caption )
        setAccessibleName( caption );
}

void LibraryNotice::showNoResults( const QString &_text )
{
    showMessage( RedesignIcons::IconSearch, Theme::Dim, qtr( "No results" ), _text,
                 qtr( "Clear search" ), QString() );
}

void LibraryNotice::showMessage( const char *icon, Theme::ColorRole role, const QString &_title,
                                 const QString &_text, const QString &primaryText,
                                 const QString &secondaryText )
{
    ring->hide();
    glyphIcon = icon;
    glyphRole = role;
    paintGlyph();
    glyph->show();
    setText( title, _title );
    setText( text, _text );
    primary->setText( primaryText );
    primary->setVisible( !primaryText.isEmpty() );
    secondary->setText( secondaryText );
    secondary->setVisible( !secondaryText.isEmpty() );
    if( accessibleName() != _title )
        setAccessibleName( _title );
}
