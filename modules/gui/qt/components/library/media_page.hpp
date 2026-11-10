/*****************************************************************************
 * media_page.hpp : the Videos and Music library pages
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

#ifndef VLC_QT_MEDIA_PAGE_HPP_
#define VLC_QT_MEDIA_PAGE_HPP_

#include "qt.hpp"

#include <QElapsedTimer>
#include <QPersistentModelIndex>
#include <QWidget>

class CardView;
class LibraryEmptyState;
class LibraryFilter;
class LibraryHeader;
class PLModel;
class QStackedWidget;
class QTimer;

/* DESIGN_SPEC 4 (D6): the video_dir or audio_dir services discovery as a
 * grid of cards, on its own PLModel so the docked playlist stays as it is.
 * The module loads when the page is first shown. Videos browse into folders
 * and play from the discovery's node, as the stock playlist does; Music
 * (P4-3) appends the album or track to the playlist and opens audio mode. */
class MediaPage : public QWidget
{
    Q_OBJECT
public:
    enum Kind { Videos, Music };
    MediaPage( intf_thread_t *, Kind, QWidget *parent = NULL );

protected:
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;

private:
    void load();
    void activate( const QModelIndex & );
    void browse( const QModelIndex &source );
    void goBack();
    void updateState();
    void setAttached( bool );
    QModelIndex rootIndex() const;

    intf_thread_t *p_intf;
    Kind kind;
    PLModel *model;
    LibraryFilter *filter;
    LibraryHeader *header;
    QStackedWidget *body;
    CardView *view;
    LibraryEmptyState *empty;
    QTimer *settle;
    int expandingId;   /* a folder being expanded, to browse into */
    bool loaded;
    /* a burst of inserted rows detaches the grid until the scan settles */
    QElapsedTimer burstClock;
    int burstRows;
    QPersistentModelIndex detachedRoot;
};

#endif
