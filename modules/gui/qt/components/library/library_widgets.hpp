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
class QTimer;
class QThreadPool;
class QToolButton;

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
     * keeps every id asked for until it became a node. */
    void expand( intf_thread_t *, PLModel *, const QModelIndex &source,
                 int *browseId, QSet<int> *requested );
    /* P4-3: appends a copy of the item (a folder: its items) to the
     * playlist and plays the first; returns that input, held, or NULL */
    input_item_t *playInPlaylist( intf_thread_t *, PLModel *, const QModelIndex &source );
    /* Once `item` (held; this takes the reference) plays, leave the library
     * for the video, or audio mode; `video` waits for the video output */
    void revealWhenPlaying( intf_thread_t *, input_item_t *, bool video );
    /* Background reads (art, file checks): never waited for, so a read
     * stuck on an offline share cannot hold up quitting */
    QThreadPool *workerPool();
}

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
 * item of each row, never decodes art in paint */
class CardDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    enum Kind { VideoCard, AlbumCard, Row };
    CardDelegate( Kind, LibraryFilter *, QObject *parent );
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

    Kind kind;
    LibraryFilter *filter;
    ArtLoader *art;
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

/* Title row of a library page: back, title, status, search field */
class LibraryHeader : public QWidget
{
    Q_OBJECT
public:
    LibraryHeader( const QString &title, QWidget *parent );
    void setFolder( const QString &name ); /* empty: the page's own title */
    void setStatus( const QString & );
    QLineEdit *search() const { return searchEdit; }

signals:
    void backRequested();

protected:
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;

private:
    QString pageTitle;
    QToolButton *back;
    QLabel *title, *status;
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

#endif
