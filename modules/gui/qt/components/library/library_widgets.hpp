/*****************************************************************************
 * library_widgets.hpp : pieces shared by the Videos, Music and Network pages
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

#ifndef VLC_QT_LIBRARY_WIDGETS_HPP_
#define VLC_QT_LIBRARY_WIDGETS_HPP_

#include "qt.hpp"
#include "util/breakpoints.hpp"
#include "util/theme.hpp"                       /* Theme::ColorRole */
#include "util/searchlineedit.hpp"              /* setupSearchField */

#include <QListView>
#include <QPersistentModelIndex>
#include <QRunnable>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QWidget>

class PLModel;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QVariantAnimation;
class QThreadPool;
class QToolButton;
class Thumbnailer;

namespace LibraryData
{
    /* A services discovery module as vlc_sd_GetNames lists it */
    struct Sd
    {
        QString name, longname;
        int category;
    };
    QList<Sd> discoveries( playlist_t * );
    /* Loads it unless it is loaded and returns its node, NULL on failure */
    playlist_item_t *loadNode( playlist_t *, const Sd & );

    /* A folder card or row: an expanded node or a not yet expanded folder */
    bool isFolder( const PLModel *, const QModelIndex &source );
    /* Lists a folder that is not expanded yet (subfolders of the scanned
     * folders, servers, shares) without playing it: the playlist then turns
     * it into a node (leafBecameParent; *browseId is the one to open then).
     * Not twice: a second listing would add the items again, so `requested`
     * keeps every id asked for until it became a node or its request is
     * over (LibraryBrowser: the timeout). */
    void expand( intf_thread_t *, PLModel *, const QModelIndex &source,
                 int *browseId, QSet<int> *requested );
    /* P4-3: appends a copy of the item (a folder: its items) to the
     * playlist and plays the first; returns that input, held, or NULL */
    input_item_t *playInPlaylist( intf_thread_t *, PLModel *, const QModelIndex &source );
    /* Once `item` (held; this takes the reference) plays, leave the library
     * for the video, or audio mode; `video` waits for the video output */
    void revealWhenPlaying( intf_thread_t *, input_item_t *, bool video );
    /* Background reads (art, Home's file checks): never waited for, so a
     * read stuck on an offline share cannot hold up quitting */
    QThreadPool *workerPool();
    /* The same for the video frames' file checks and folder listings, on
     * a few threads of their own: a share that hangs blocks only these */
    QThreadPool *filePool();
}

class CardView;
class LibraryHeader;
class LibraryNotice;
class LibraryFilter;

/* Browsing a PLModel's folders in a CardView, shared by the Videos, Music
 * and Network pages: into a folder (listing it first when it is not yet),
 * back up to the page's top, the search cleared per folder, and the
 * listing's loading and failed states. The page keeps its own states. */
class LibraryBrowser : public QObject
{
    Q_OBJECT
public:
    LibraryBrowser( intf_thread_t *, LibraryFilter *, CardView *, LibraryHeader *, QObject *parent );
    /* where back stops (backFromTop then); invalid: the model's root */
    void setTop( const QModelIndex &source ) { top = source; }
    void browse( const QModelIndex &source );
    /* lists a folder not expanded yet, then browses into it */
    void expand( const QModelIndex &source );
    void cancelExpand();
    bool expanding() const;
    bool failed() const { return expandFailed; }
    /* the loading or failed notice while a folder is listed; false when
     * none is. `cannotList` has %1 for the folder's name, else `name`. */
    bool showExpandState( LibraryNotice *, const QString &cannotList, const QString &name ) const;

public slots:
    void goBack();
    /* the notice's buttons: try again, back, clear the search */
    void noticeAction( bool primary );

signals:
    void aboutToBrowse();
    void changed();     /* the page updates its state */
    void backFromTop();

private:
    PLModel *model() const;

    intf_thread_t *p_intf;
    LibraryFilter *filter;
    CardView *view;
    LibraryHeader *header;
    QPersistentModelIndex top;
    QTimer *expandTimer;    /* then the folder is reported as not opened */
    int expandingId;        /* a folder being expanded, to browse into */
    QPersistentModelIndex expandingIndex;
    QSet<int> expandRequested;
    bool expandFailed;
};

/* Search for the library grids: filters the rows directly under the browsed
 * folder by title, artist and album. Never PLModel::filter, which changes
 * the shared playlist items. Also names the rows (accessibility, type-ahead). */
class LibraryFilter : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit LibraryFilter( QObject *parent );
    void setBrowseRoot( const QModelIndex &source );
    void setText( const QString & );
    input_item_t *inputItem( const QModelIndex &proxy ) const;
    QVariant data( const QModelIndex &, int role ) const Q_DECL_OVERRIDE;

protected:
    bool filterAcceptsRow( int, const QModelIndex & ) const Q_DECL_OVERRIDE;

private:
    QPersistentModelIndex root;
    QString text;
};

/* One cover read and scaled on LibraryData::workerPool(); the result goes
 * through a queued signal, so it is dropped when the loader is gone */
class ArtJob : public QObject, public QRunnable
{
    Q_OBJECT
public:
    ArtJob( const QString &_path, const QSize &_size, const QString &_key )
        : path( _path ), size( _size ), key( _key ) {}
    void run() Q_DECL_OVERRIDE;

signals:
    void loaded( const QString &key, const QImage & );

private:
    QString path;
    QSize size;
    QString key;
};

/* Cover art read and scaled in a worker thread, kept in QPixmapCache */
class ArtLoader : public QObject
{
    Q_OBJECT
public:
    explicit ArtLoader( QObject *parent );
    /* The art scaled to cover `size` (device px), or a null pixmap while it
     * loads or when there is none: ready() follows a successful load */
    QPixmap get( const QString &path, const QSize &size );

signals:
    void ready();

private slots:
    void loaded( const QString &key, const QImage & );

private:
    QSet<QString> pending, failed;
};

/* Cards (Videos, Music) or rows (network folders); paints from the input
 * item of each row, never decodes art in paint. With a Thumbnailer (Videos)
 * a card without art shows a frame of its video, a folder's the frame of
 * its first video and its video count, and both how far they were watched. */
class CardDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum Kind { VideoCard, AlbumCard, Row };
    CardDelegate( Kind, LibraryFilter *, QObject *parent, Thumbnailer * = NULL );
    void setColumns( int cols, int cellW );
    int cardHeight( int cardW ) const;
    void paint( QPainter *, const QStyleOptionViewItem &, const QModelIndex & ) const Q_DECL_OVERRIDE;
    QSize sizeHint( const QStyleOptionViewItem &, const QModelIndex & ) const Q_DECL_OVERRIDE;

signals:
    void repaint();

private:
    void paintCard( QPainter *, const QStyleOptionViewItem &, const QModelIndex & ) const;
    void paintRow( QPainter *, const QStyleOptionViewItem &, const QModelIndex & ) const;
    QString metaLine( input_item_t *, const QModelIndex & ) const;
    QString artPath( input_item_t *, const QModelIndex & ) const;
    /* 0..1 watched, -1 for no bar */
    qreal progress( input_item_t *, const QModelIndex & ) const;

    Kind kind;
    LibraryFilter *filter;
    ArtLoader *art;
    Thumbnailer *thumbs;
    int cols, cellW;
};

/* The grid (columns by breakpoint) or list of a library page. Single click
 * or Enter activates; Backspace or Esc asks to go back. */
class CardView : public QListView
{
    Q_OBJECT
public:
    /* columns per breakpoint XS, S, M, L; NULL for a list */
    CardView( CardDelegate *, const int *columns, QWidget *parent );
    void setBackEnabled( bool b ) { backEnabled = b; }

signals:
    void cardActivated( const QModelIndex & );
    void backRequested();

protected:
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;
    void mousePressEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void mouseReleaseEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent( QMouseEvent * ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;

private:
    void updateGrid();

    CardDelegate *delegate;
    const int *columns;
    QPersistentModelIndex pressed;
    bool backEnabled;
};

/* Title row of a library page: back, title, search field */
class LibraryHeader : public QWidget
{
    Q_OBJECT
public:
    LibraryHeader( const QString &title, QWidget *parent );
    void setFolder( const QString &name ); /* empty: the page's own title */
    QLineEdit *search() const { return searchEdit; }

signals:
    void backRequested();

protected:
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;

private:
    QString pageTitle;
    QToolButton *back;
    QLabel *title;
    QLineEdit *searchEdit;
};

/* DESIGN_SPEC 4: "Your library is empty", Add a folder (P4-2), Open media;
 * Home and the library pages, each with its own text and an optional note */
class LibraryEmptyState : public QWidget
{
    Q_OBJECT
public:
    LibraryEmptyState( const QString &text, const QString &note, QWidget *parent );
    void setNoteVisible( bool );
    QSize sizeHint() const Q_DECL_OVERRIDE;

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    QLabel *noteLabel;
};

/* The library's loading indicator: the buffering ring's accent arc turning
 * on an ink track, only while shown; still when Windows animations are off */
class LoadingRing : public QWidget
{
    Q_OBJECT
public:
    explicit LoadingRing( QWidget *parent );
    QSize sizeHint() const Q_DECL_OVERRIDE;

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;

private:
    QVariantAnimation *spin;
};

/* A page's or folder's state in the middle of its content area: loading
 * (the ring and a caption), or a glyph, a title, a text and up to two
 * buttons (empty folder, no results, could not be opened) */
class LibraryNotice : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryNotice( QWidget *parent );
    void showLoading( const QString &caption );
    /* a search that matches nothing: `text` says so; Clear search */
    void showNoResults( const QString &text );
    /* an empty button text hides that button */
    void showMessage( const char *icon, Theme::ColorRole, const QString &title, const QString &text,
                      const QString &primary, const QString &secondary );

signals:
    void primaryClicked();
    void secondaryClicked();

private:
    void setText( QLabel *, const QString & );
    void paintGlyph();

    LoadingRing *ring;
    const char *glyphIcon;      /* the last message's, NULL before one */
    Theme::ColorRole glyphRole;
    QLabel *glyph, *title, *text;
    QPushButton *primary, *secondary;
};

/* "2010 · 1080p · H.264 · 5.1": what preparsing found so far of an item's
 * year, picture height, video codec and surround channels; empty if none */
QString mediaMetaLine( input_item_t * );

/* The item's or MRL's local file or folder, empty when it is not one */
QString localPath( input_item_t * );
QString localPath( const QString &mrl );

/* Whether the path's extension is in `filters` ("*.mkv"...: EXTENSIONS_*) */
bool hasExtension( const QString &path, const QStringList &filters );

/* " · ": between the parts of a meta line */
QString metaSeparator();

/* A card's progress (0..1) along the bottom of its picture */
void paintProgress( QPainter *, const QRectF &thumb, qreal fraction, Qt::LayoutDirection );

/* A card's picture, cropped to fill the rounded `thumb`, or the striped
 * placeholder with its glyph; leaves the painter clipped to `thumb` */
void paintCardThumb( QPainter *, const QRectF &thumb, qreal radius, QPixmap,
                     const char *glyph, int stripePx, int glyphPx );

/* A time pill in the thumb's bottom end corner (left in right-to-left) */
void paintTimeBadge( QPainter *, const QRectF &thumb, const QString &, const QFont &,
                     Qt::LayoutDirection, int insetX, int insetY, int padX, int padY );

#endif
